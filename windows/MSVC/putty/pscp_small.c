#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#include "putty.h"


#define TIME_WIN_TO_POSIX(ft, t) do { \
    ULARGE_INTEGER uli; \
    uli.LowPart  = (ft).dwLowDateTime; \
    uli.HighPart = (ft).dwHighDateTime; \
    uli.QuadPart = uli.QuadPart / 10000000ull - 11644473600ull; \
    (t) = (unsigned long) uli.QuadPart; \
} while(0)

/*
 * The maximum amount of queued data we accept before we stop and
 * wait for the server to process some.
 */
#define MAX_SCP_BUFSIZE 16384

static Backend *back;
static void *backhandle;
static int errs = 0;
int sent_eof = FALSE;

static unsigned char *outptr;	       /* where to put the data */
static unsigned outlen;		       /* how much data required */
static unsigned char *pending = NULL;  /* any spare data */
static unsigned pendlen = 0, pendsize = 0;	/* length and phys. size of buffer */

struct RFile {
    HANDLE h;
};


int do_eventsel_loop()
{
    int n, nhandles, nallhandles, netindex = 0, error;
    unsigned long next, then;
    long ticks;
    HANDLE *handles;
    unsigned long now = GETTICKCOUNT();
	MSG msg;

    if (run_timers(now, &next)) {
	then = now;
	now = GETTICKCOUNT();
	if (now - then > next - then)
	    ticks = 0;
	else
	    ticks = next - now;
    } else {
	ticks = INFINITE;
    }

    handles = handle_get_events(&nhandles);
    handles = sresize(handles, nhandles+2, HANDLE);
    nallhandles = nhandles;
   
	n = MsgWaitForMultipleObjects(nallhandles, handles, FALSE, ticks, QS_ALLINPUT);

	error = GetLastError();

    if ((unsigned)(n - WAIT_OBJECT_0) < (unsigned)nhandles) {
	handle_got_event(handles[n - WAIT_OBJECT_0]);
    } else if (netindex >= 0 && n == WAIT_OBJECT_0 + netindex) {
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			DispatchMessage(&msg);
			check_and_enact_pending_netevent();
		}
	}

    sfree(handles);

	return 0;
}

int init_pscp_small(Backend *_back, void *_backhandle, HANDLE _hwnd)
{
	back = _back;
	backhandle = _backhandle;
}

RFile* open_existing_file(char *name, uint64 *size,
			  unsigned long *mtime, unsigned long *atime,
                          long *perms)
{
	 HANDLE h;
	 RFile* ret; 
	 h = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL,
		   OPEN_EXISTING, 0, 0);

	if (h == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		return NULL;
	}

	ret = snew(RFile);
    ret->h = h;

	if (size)
        size->lo=GetFileSize(h, &(size->hi));

    if (mtime || atime) {
	FILETIME actime, wrtime;
	GetFileTime(h, NULL, &actime, &wrtime);
	if (atime)
	    TIME_WIN_TO_POSIX(actime, *atime);
	if (mtime)
	    TIME_WIN_TO_POSIX(wrtime, *mtime);
    }

    if (perms)
        *perms = -1;

	return ret;
}


int read_from_file(RFile *f, void *buffer, int length)
{
    int ret;
    DWORD read;
    ret = ReadFile(f->h, buffer, length, &read, NULL);
    if (!ret)
	return -1;		       /* error */
    else
	return read;
}

void close_rfile(RFile *f)
{
    CloseHandle(f->h);
    sfree(f);
}

int scp_send_filetimes(unsigned long mtime, unsigned long atime)
{
	char buf[80];
	sprintf(buf, "T%lu 0 %lu 0\n", mtime, atime);
	back->ssh_send_secondary_channel(backhandle,buf, strlen(buf));
	return 1;
}

int scp_send_filename(char *name, uint64 size, int permissions)
{
	char buf[40];
	char sizestr[40];
	uint64_decimal(size, sizestr);
        if (permissions < 0)
            permissions = 0644;
	sprintf(buf, "C%04o %s ", (int)(permissions & 07777), sizestr);
	back->ssh_send_secondary_channel(backhandle, buf, strlen(buf));
	back->ssh_send_secondary_channel(backhandle, name, strlen(name));
	back->ssh_send_secondary_channel(backhandle, "\n", 1);
	return 1;
}

int scp_send_filedata(char *data, int len)
{
	int bufsize = back->ssh_send_secondary_channel(backhandle, data, len);

	/*
	 * If the network transfer is backing up - that is, the
	 * remote site is not accepting data as fast as we can
	 * produce it - then we must loop on network events until
	 * we have space in the buffer again.
	 */
	while (bufsize > MAX_SCP_BUFSIZE) {
	    do_eventsel_loop();
		bufsize = back->sendbuffer(backhandle);
	}
	return 0;
}

int scp_send_finish(void)
{
	back->ssh_send_secondary_channel(backhandle, "", 1);
	return 1;
}


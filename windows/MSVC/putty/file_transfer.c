#include <stdio.h>
#include <ctype.h>

#include "putty.h"
#include "ssh.h"

static char src_path[MAX_PATH];
static Backend *back;
static void *backhandle;

Ftransfer * ftransfer_create(Backend *_back, void *_backhandle, HANDLE _hwnd)
{
	Ftransfer * ftransfer = snew(struct ftransfer_tag);
	back = _back;
	backhandle = _backhandle;
	init_pscp_small(_back, _backhandle, _hwnd);
	return ftransfer;
}

void do_file_transfer(void *handle)
{
	char * pwd = "pwd\r";
	Ftransfer *ftransfer = (Ftransfer*)handle;
	back->ssh_open_second_channel(backhandle);

	memset(src_path,0,MAX_PATH);
	memcpy(src_path,ftransfer->source_path,strlen(ftransfer->source_path));
	back->ssh_send_non_terminal_data(backhandle,pwd,strlen(pwd));
}

int non_term_data(const char *data, int len)
{
	unsigned long c;
	int nchars = len;
	char *localbuf = snewn(len,char);
	char *headbuf = localbuf;
	int start_pos = 0;
	const char * pwd = "pwd\r\n";
	int pre_len = strlen(pwd);
	char dest_path[1024];
	int i = 0;
	

	memcpy(localbuf,data,len);

	
	if (strncmp(pwd,localbuf,pre_len) == 0) {
		localbuf+=pre_len;
		nchars-=pre_len;
	}
	while(nchars > 0 && *localbuf != '\r') {
		c = *localbuf++;
		dest_path[i++] = c;
		nchars--;
	}
	dest_path[i++] = '\0';

	
	back->ssh_send_scp(backhandle,dest_path,src_path);

	sfree(headbuf);
}
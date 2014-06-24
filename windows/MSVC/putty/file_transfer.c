#include <stdio.h>
#include <ctype.h>

#include "putty.h"
#include "ssh.h"

static char src_path[MAX_PATH];
static Backend *back;
static void *backhandle;
static int got_path;

Ftransfer * ftransfer_create(Backend *_back, void *_backhandle, HANDLE _hwnd)
{
	Ftransfer * ftransfer = snew(struct ftransfer_tag);
	back = _back;
	backhandle = _backhandle;
	init_pscp_small(_back, _backhandle, _hwnd);
	return ftransfer;
}

void do_file_transfer(void *handle, char * osc_string, int osc_strlen)
{
	Ftransfer *ftransfer = (Ftransfer*)handle;
	back->ssh_open_second_channel(backhandle);
	memset(src_path,0,MAX_PATH);
	memcpy(src_path,ftransfer->source_path,strlen(ftransfer->source_path));
	got_path = FALSE;
	if(FALSE){
	} 
	else
	{
		char * pwd = "pwd\r";
		back->ssh_send_non_terminal_data(backhandle,pwd,strlen(pwd));
	}
}

int non_term_data(const char *data, int len, char * osc_string, Conf *conf)
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
	char * pch;
	char username_t[256];
	char * username = back->ssh_get_remote_username(backhandle);
	
	memcpy(localbuf,data,len);

	if(strstr(localbuf,"Command not found.") != NULL) {
		non_terminal_data = 0;
		nonfatal("error occurred while upload, please try again.");
		return 1;
	}
		

	if (!got_path) {
		if (strncmp(pwd,localbuf,pre_len) == 0 && (pch = strchr(localbuf,'/')) != NULL) {
			localbuf+=pre_len;
			nchars-=pre_len;
	
			while(nchars > 0 && (*localbuf != '\r' && *localbuf != '\n')) {
				c = *localbuf++;
				dest_path[i++] = c;
				nchars--;
			}
			got_path = TRUE;
		}
		// the response not starts with "pwd\r\n"
		else if ((pch = strchr(localbuf,'/')) != NULL) {
				while(nchars > 0 && (*pch != '\r' && *pch != '\n')) {
					c = *pch++;
					dest_path[i++] = c;
					nchars--;
				}
			got_path = TRUE;
		}
		dest_path[i++] = '\0';

		if(got_path)
			back->ssh_send_scp(backhandle,dest_path,src_path);

		sfree(headbuf);
	}
	return 1;
}
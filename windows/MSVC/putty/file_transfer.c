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

void do_file_transfer(Ftransfer *ftransfer, char ** src_path_arr, int file_count)
{
	back->ssh_scp_progress(backhandle, src_path_arr, file_count);
}

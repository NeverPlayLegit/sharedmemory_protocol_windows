#pragma once
#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

typedef struct sh_client_t sh_client_t;

typedef void (*_sh_client_recv_callback)(sh_client_t* sh, int8_t *buf, uint32_t len);

struct sh_client_t {
	HANDLE hBuffer_read;
	HANDLE hBuffer_write;
	HANDLE hThread_recv;
	int8_t* buf_read, * buf_write;
	size_t buf_len;
	uint8_t _active;
	_sh_client_recv_callback func_callback;
};

DWORD WINAPI _sh_client_task_recv(void *data) {
	sh_client_t *sh = (sh_client_t*)data;
	while (sh->_active) {
		if (sh->buf_read[0] == 1) {
			uint32_t len = sh->buf_read[1] | (uint32_t)sh->buf_read[2] << 8 | (uint32_t)sh->buf_read[3] << 16 | (uint32_t)sh->buf_read[4] << 24;
			int8_t* buf = (int8_t*)malloc(len);
			memcpy(buf, &sh->buf_read[5], len);
			sh->func_callback(sh, buf, len);
			memset(sh->buf_read, 0, sh->buf_len);
			sh->buf_read[0] = 0;
		}
		Sleep(1);
	}
	return 0;
}

/*
Sends buffer to sharedmemory-connection

Blocks until buffer read (read flag ist set)
*/
void sh_client_send(sh_client_t *sh, int8_t *buf, uint32_t len) {
	if (sh->_active && len + 5 < sh->buf_len) {
		while (sh->buf_write[0] != 0) Sleep(1);
		
		memcpy(&sh->buf_write[5], buf, len);
		
		sh->buf_write[4] = (len >> 24) & 0xFF;
		sh->buf_write[3] = (len >> 16) & 0xFF;
		sh->buf_write[2] = (len >> 8) & 0xFF;
		sh->buf_write[1] = len & 0xFF;

		sh->buf_write[0] = 1;
	}
}

sh_client_t* sh_client_new(_sh_client_recv_callback callback, const char* shared_name_read, const char* shared_name_write, size_t buf_len) {
	sh_client_t* res = (sh_client_t*)malloc(sizeof(sh_client_t));
	res->buf_len = buf_len;
	res->func_callback = callback;
	
	res->hBuffer_read = CreateFileMapping(INVALID_HANDLE_VALUE,
		NULL, PAGE_READWRITE, 0, buf_len, shared_name_read);
	if (res->hBuffer_read == NULL) {
		goto err;
	}

	res->hBuffer_write = CreateFileMapping(INVALID_HANDLE_VALUE,
		NULL, PAGE_READWRITE, 0, buf_len, shared_name_write);
	if (res->hBuffer_write == NULL) {
		CloseHandle(res->hBuffer_read);
		goto err;
	}

	res->buf_read = (int8_t*)MapViewOfFile(res->hBuffer_read, FILE_MAP_ALL_ACCESS,
		0, 0, buf_len);
	if (res->buf_read == NULL) {
		CloseHandle(res->hBuffer_read);
		CloseHandle(res->hBuffer_write);
		goto err;
	}

	res->buf_write = (int8_t*)MapViewOfFile(res->hBuffer_write, FILE_MAP_ALL_ACCESS,
		0, 0, buf_len);
	if (res->buf_write == NULL) {
		UnmapViewOfFile(res->buf_read);
		CloseHandle(res->hBuffer_read);
		CloseHandle(res->hBuffer_write);
		goto err;
	}

	memset(res->buf_read, 0, res->buf_len);
	memset(res->buf_write, 0, res->buf_len);

	res->_active = 1;
	res->hThread_recv = CreateThread(0, 0, _sh_client_task_recv, res, 0, 0);

	return res;

err:
	free(res);
	return 0;
}

void sh_client_close(sh_client_t *sh) {
	sh->_active = 0;
	WaitForSingleObject(sh->hThread_recv, INFINITE);
	UnmapViewOfFile(sh->buf_read);
	UnmapViewOfFile(sh->buf_write);
	CloseHandle(sh->hBuffer_read);
	CloseHandle(sh->hBuffer_write);
	free(sh);
}

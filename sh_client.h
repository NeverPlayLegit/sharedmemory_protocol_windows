#pragma once
#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

typedef struct sh_client_t sh_client_t;

typedef void (*_sh_client_recv_callback)(sh_client_t* sh, int8_t* buf, uint32_t len);

struct sh_client_t {
	HANDLE hBuffer_read;
	HANDLE hBuffer_write;
	HANDLE hThread_recv;
	HANDLE hEvent_read, hEvent_read2;
	HANDLE hEvent_write, hEvent_write2;
	int8_t* buf_read, * buf_write;
	size_t buf_len;
	uint8_t _active;
	_sh_client_recv_callback func_callback;
};

DWORD WINAPI _sh_client_task_recv(void* data) {
	sh_client_t* sh = (sh_client_t*)data;
	while (sh->_active) {
		WaitForSingleObject(sh->hEvent_read2, INFINITE);
		uint32_t len = sh->buf_read[1] | (uint32_t)sh->buf_read[2] << 8
			| (uint32_t)sh->buf_read[3] << 16 | (uint32_t)sh->buf_read[4] << 24;
		int8_t* buf = (int8_t*)malloc(len);
		memcpy(buf, &sh->buf_read[5], len);
		sh->func_callback(sh, buf, len);
		memset(sh->buf_read, 0, sh->buf_len);
		SetEvent(sh->hEvent_read);
	}
	return 0;
}

/*
Sends buffer to sharedmemory-connection

Blocks until buffer read (read flag ist set)
*/
void sh_client_send(sh_client_t* sh, int8_t* buf, uint32_t len) {
	if (sh->_active && len + 5 < sh->buf_len) {
		WaitForSingleObject(sh->hEvent_write, INFINITE);
		memcpy(&sh->buf_write[5], buf, len);

		sh->buf_write[4] = (len >> 24) & 0xFF;
		sh->buf_write[3] = (len >> 16) & 0xFF;
		sh->buf_write[2] = (len >> 8) & 0xFF;
		sh->buf_write[1] = len & 0xFF;

		sh->buf_write[0] = 1;

		SetEvent(sh->hEvent_write2);
	}
}

void sh_client_close(sh_client_t* sh) {
	if (!sh) return;
	sh->_active = 0;
	if(sh->hThread_recv)WaitForSingleObject(sh->hThread_recv, INFINITE);
	if(sh->buf_read)UnmapViewOfFile(sh->buf_read);
	if (sh->buf_write)UnmapViewOfFile(sh->buf_write);
	if(sh->hBuffer_read)CloseHandle(sh->hBuffer_read);
	if(sh->hBuffer_write)CloseHandle(sh->hBuffer_write);
	if (sh->hEvent_read)CloseHandle(sh->hEvent_read);
	if (sh->hEvent_read2)CloseHandle(sh->hEvent_read2);
	if (sh->hEvent_write)CloseHandle(sh->hEvent_write);
	if (sh->hEvent_write2)CloseHandle(sh->hEvent_write2);
	free(sh);
}

sh_client_t * sh_client_new(_sh_client_recv_callback callback, const char* shared_name_read, const char* shared_name_write
	, const char* event_name_read, const char* event_name_write
	, const char* event_name_read2, const char* event_name_write2, size_t buf_len) {
	sh_client_t* res = (sh_client_t*)malloc(sizeof(sh_client_t));
	res->buf_len = buf_len;
	res->func_callback = callback;

	res->hBuffer_read = CreateFileMapping(INVALID_HANDLE_VALUE,
		NULL, PAGE_READWRITE, 0, buf_len, shared_name_read);
	if (res->hBuffer_read == NULL) {
		goto dispose;
	}

	res->hBuffer_write = CreateFileMapping(INVALID_HANDLE_VALUE,
		NULL, PAGE_READWRITE, 0, buf_len, shared_name_write);
	if (res->hBuffer_write == NULL) {
		goto dispose;
	}

	res->buf_read = (int8_t*)MapViewOfFile(res->hBuffer_read, FILE_MAP_ALL_ACCESS,
		0, 0, buf_len);
	if (res->buf_read == NULL) {
		goto dispose;
	}

	res->buf_write = (int8_t*)MapViewOfFile(res->hBuffer_write, FILE_MAP_ALL_ACCESS,
		0, 0, buf_len);
	if (res->buf_write == NULL) {
		goto dispose;
	}

	res->hEvent_read = CreateEvent(NULL, FALSE, FALSE, event_name_read);
	if (res->hEvent_read == NULL) {
		goto dispose;
	}

	res->hEvent_read2 = CreateEvent(NULL, FALSE, FALSE, event_name_read2);
	if (res->hEvent_read2 == NULL) {
		goto dispose;
	}

	res->hEvent_write = CreateEvent(NULL, FALSE, FALSE, event_name_write);
	if (res->hEvent_write == NULL) {
		goto dispose;
	}

	res->hEvent_write2 = CreateEvent(NULL, FALSE, FALSE, event_name_write2);
	if (res->hEvent_write2 == NULL) {
		goto dispose;
	}

	SetEvent(res->hEvent_write);
	SetEvent(res->hEvent_read);
	ResetEvent(res->hEvent_write2);
	ResetEvent(res->hEvent_read2);
	memset(res->buf_read, 0, res->buf_len);
	memset(res->buf_write, 0, res->buf_len);

	res->_active = 1;
	res->hThread_recv = CreateThread(0, 0, _sh_client_task_recv, res, 0, 0);

	return res;

dispose:
	sh_client_close(res);
	return 0;
}


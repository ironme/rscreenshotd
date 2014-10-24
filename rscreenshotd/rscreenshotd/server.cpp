#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#include <stdlib.h>
#include <time.h>
#include <queue>
#include <utility>
//#include <stdio.h>

#include "screenshot.h"
#include "compress.h"

// Need to link with Ws2_32.lib

#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_PORT "1438"
#define MAX_SERVER_THREADS 128
#define CLOSE_WAIT_SEC 120

#define STD_REQUEST "RSCREENSHOTD: request screenshot."
#define MAX_REQUEST_LEN 128 // MUST > strlen(STD_REQUEST);
#define BAD_REQUEST_MSG "ERROR: this is a remote screenshot server. (rscreenshotd, developed by ZBY)"

struct thread_data {
	void *bmpfile;
	void *lzofile;
	SOCKET socket;
	HANDLE handle;
	DWORD id;
};

static CRITICAL_SECTION ScreenshotCriticalSection;
static CRITICAL_SECTION QueueCriticalSection;
static thread_data tdata[MAX_SERVER_THREADS];

static std::queue<std::pair<SOCKET, time_t> > wait_queue;

static void wait_queue_clear()
{
	EnterCriticalSection(&QueueCriticalSection);
	while (!wait_queue.empty()) {
		closesocket(wait_queue.front().first);
		wait_queue.pop();
	}
	LeaveCriticalSection(&QueueCriticalSection);
}

static void wait_queue_pop()
{
	EnterCriticalSection(&QueueCriticalSection);
	time_t deadline = time(NULL) - CLOSE_WAIT_SEC;
	while (!wait_queue.empty() && wait_queue.front().second <= deadline) {
		closesocket(wait_queue.front().first);
		wait_queue.pop();
	}
	LeaveCriticalSection(&QueueCriticalSection);
}

static void wait_queue_push(SOCKET ClientSocket)
{
	EnterCriticalSection(&QueueCriticalSection);
	wait_queue.push(std::make_pair(ClientSocket, time(NULL)));
	LeaveCriticalSection(&QueueCriticalSection);
}

static DWORD server_thread(LPVOID param)
{
	int iSendResult;
	int iResult;
	thread_data *ptr = (thread_data *) param;
	SOCKET ClientSocket = ptr->socket;
	
	char request[MAX_REQUEST_LEN];
	char *request_ptr = request;
	while (request_ptr - request < MAX_REQUEST_LEN) {
		iResult = recv(ClientSocket, request_ptr, request + MAX_REQUEST_LEN - request_ptr, 0);
		if (iResult <= 0) {
			closesocket(ClientSocket);
			return 1;
		}
		request_ptr += iResult;
	}
	if (memcmp(request, STD_REQUEST, strlen(STD_REQUEST)) != 0) {
		iSendResult = send(ClientSocket, BAD_REQUEST_MSG, strlen(BAD_REQUEST_MSG), 0);
		if (iSendResult == SOCKET_ERROR) {
			//printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			return 1;
		}
		goto done;
	}

	size_t bmpsize, lzosize;
	EnterCriticalSection(&ScreenshotCriticalSection);
	bmpsize = screenshot(&ptr->bmpfile);
	lzosize = compress(&ptr->lzofile, ptr->bmpfile, bmpsize); 
	LeaveCriticalSection(&ScreenshotCriticalSection);

	char *data = (char *) ptr->lzofile;
	size_t len = lzosize;
	do {
		iSendResult = send(ClientSocket, data, len, 0);
		if (iSendResult == SOCKET_ERROR) {
			//printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			return 1;
		}
		len -= iSendResult;
		data += iSendResult;
	} while (len > 0);

done:
	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		//printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}
	wait_queue_push(ClientSocket);
	return 0;
}

static void init_tdata()
{
	InitializeCriticalSection(&ScreenshotCriticalSection);
	InitializeCriticalSection(&QueueCriticalSection);
	for (int i = 0; i < MAX_SERVER_THREADS; i++) {
		tdata[i].bmpfile = tdata[i].lzofile = NULL;
		tdata[i].socket = INVALID_SOCKET;
		tdata[i].handle = NULL;
	}
}

static void clean_tdata()
{
	for (int i = 0; i < MAX_SERVER_THREADS; i++)
		if (tdata[i].handle) {
			WaitForSingleObject(tdata[i].handle, INFINITE);
			CloseHandle(tdata[i].handle);
		}
	for (int i = 0; i < MAX_SERVER_THREADS; i++) {
		if (tdata[i].bmpfile) free(tdata[i].bmpfile);
		if (tdata[i].lzofile) free(tdata[i].lzofile);
	}
	wait_queue_clear();
	DeleteCriticalSection(&QueueCriticalSection);
	DeleteCriticalSection(&ScreenshotCriticalSection);
}

static thread_data * find_avaliable_tdata()
{
	thread_data *ret = NULL;
	static HANDLE thandle[MAX_SERVER_THREADS];

	int tcnt = 0;
	for (int i = 0; i < MAX_SERVER_THREADS; i++)
		if (tdata[i].handle != NULL)
			thandle[tcnt++] = tdata[i].handle;
	if (tcnt == MAX_SERVER_THREADS)
		WaitForMultipleObjects(tcnt, thandle, FALSE, INFINITE);

	for (int i = 0; i < MAX_SERVER_THREADS; i++)
		if (tdata[i].handle == NULL)
			ret = &tdata[i];
		else {
			DWORD exit_code;
			GetExitCodeThread(tdata[i].handle, &exit_code);
			if (exit_code != STILL_ACTIVE) {
				CloseHandle(tdata[i].handle);
				tdata[i].handle = NULL;
				ret = &tdata[i];
			}
		}
	return ret;
}

int server(void) 
{
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;
    
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        //printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        //printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        //printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        //printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        //printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

	init_tdata();

	while (1) {
		// Accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			//printf("accept failed with error: %d\n", WSAGetLastError());
			clean_tdata();
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
		
		wait_queue_pop();
		thread_data *ptr = find_avaliable_tdata();
		ptr->socket = ClientSocket;
		ptr->handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) server_thread, (LPVOID) ptr, 0, &ptr->id);
	}
}

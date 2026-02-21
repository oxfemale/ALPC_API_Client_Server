#include <Windows.h>
#include <winternl.h>
#include <stdio.h>
#include "ntalpcapi.h"

#pragma comment(lib, "ntdll.lib")

#define MAX_MSG_LEN 0x500

// Create a memory buffer for the message to be sent to the server. This buffer will contain the PORT_MESSAGE struct followed by the message data.
LPVOID AllocMsgMem(SIZE_T Size)
{
    /*
        It's important to understand that after the PORT_MESSAGE struct is the message data
    */
    return(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Size + sizeof(PORT_MESSAGE)));
}

void CreatePortAndListen(LPCWSTR PortName)
{
    ALPC_PORT_ATTRIBUTES    serverPortAttr;
    OBJECT_ATTRIBUTES       objPort;
    UNICODE_STRING          usPortName;
    PORT_MESSAGE            pmRequest;
    PORT_MESSAGE            pmReceive;
    NTSTATUS                ntRet;
    BOOLEAN                 bBreak;
    HANDLE                  hConnectedPort;
    HANDLE                  hPort;
    SIZE_T                  nLen;
    LPVOID                  lpMem;
    BYTE                    bTemp;

    RtlInitUnicodeString(&usPortName, PortName);
    InitializeObjectAttributes(&objPort, &usPortName, 0, 0, 0);
    RtlSecureZeroMemory(&serverPortAttr, sizeof(serverPortAttr));
    serverPortAttr.MaxMessageLength = MAX_MSG_LEN; // For ALPC this can be max of 64KB

	// Create the ALPC port
    ntRet = NtAlpcCreatePort(&hPort, &objPort, &serverPortAttr);
    printf("[i] NtAlpcCreatePort: 0x%X\n", ntRet);
    if (!ntRet)
    {
        nLen = sizeof(pmReceive);
		// Wait for a client to connect and receive the connection request message. The connection request message will be stored in the pmReceive variable.
        ntRet = NtAlpcSendWaitReceivePort(hPort, 0, NULL, NULL, &pmReceive, &nLen, NULL, NULL);
        if (!ntRet)
        {
            RtlSecureZeroMemory(&pmRequest, sizeof(pmRequest));
            pmRequest.MessageId = pmReceive.MessageId;
            pmRequest.u1.s1.DataLength = 0x0;
            pmRequest.u1.s1.TotalLength = pmRequest.u1.s1.DataLength + sizeof(PORT_MESSAGE);
			// Accept the connection request from the client. This will create a new port handle for communication with the client, which will be stored in the hConnectedPort variable.
            ntRet = NtAlpcAcceptConnectPort(&hConnectedPort, hPort, 0, NULL, NULL, NULL, &pmRequest, NULL, TRUE); // 0
            printf("[i] NtAlpcAcceptConnectPort: 0x%X\n", ntRet);
            if (!ntRet)
            {
                bBreak = TRUE;
                while (bBreak)
                {
                    nLen = MAX_MSG_LEN;
                    lpMem = AllocMsgMem(nLen);
					// Wait for a message from the client. The message will be stored in the same memory buffer that was allocated for sending messages to the server.
                    NtAlpcSendWaitReceivePort(hPort, 0, NULL, NULL, (PPORT_MESSAGE)lpMem, &nLen, NULL, NULL);
                    pmReceive = *(PORT_MESSAGE*)lpMem;
                    if (!strcmp((const char*)(BYTE*)lpMem + sizeof(PORT_MESSAGE), "exit\n"))
                    {
                        printf("[i] Received 'exit' command\n");
                        HeapFree(GetProcessHeap(), 0, lpMem);
						// Disconnect the client from the server and close the port handles
                        ntRet = NtAlpcDisconnectPort(hPort, 0);
                        printf("[i] NtAlpcDisconnectPort: %X\n", ntRet);
                        CloseHandle(hConnectedPort);
                        CloseHandle(hPort);
                        ExitThread(0);
                    }
                    else
                    {
                        printf("[i] Received Data: ");
                        for (int i = 0; i <= pmReceive.u1.s1.DataLength; i++)
                        {
                            bTemp = *(BYTE*)((BYTE*)lpMem + i + sizeof(PORT_MESSAGE));
                            printf("0x%X ", bTemp);
                        }
                        printf("\n");
                        HeapFree(GetProcessHeap(), 0, lpMem);
                    }
                }
            }
        }
    }
    ExitThread(0);
}

void main()
{
    HANDLE hThread;

    printf("[i] ALPC-Example Server\n");
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&CreatePortAndListen, (LPVOID)L"\\RPC Control\\NameOfPort", 0, NULL);
    WaitForSingleObject(hThread, INFINITE);
    printf("[!] Shuting down server\n");
    getchar();
    return;
}

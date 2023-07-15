#pragma once
#define MAXSTRING 1024

typedef struct _RPC_INFO {
	char szServiceEndpoint[MAXSTRING];
	char szProtocol[MAXSTRING];
	char szNetworkAddress[MAXSTRING];
	char szID[MAXSTRING];
	char szUUID[MAXSTRING];
	int iMajorVersion;
	int iMinorVersion;
	char szAnnotation[MAXSTRING];
	int iStatus = -1;
} RPC_INFO, *p_RPC_INFO;
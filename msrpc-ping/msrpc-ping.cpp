// msrpc-ping.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"

#include <ws2tcpip.h>
#include <rpc.h>
#include "msrpc-ping.h"

#define MAXSTRING 1024
#define MAXRPCSERVICES 50000

RPC_INFO rpc_info_database[MAXRPCSERVICES];
int irpc_info_database_count;
int iVerbose = 0;

char * replace_string( char szBaseString[], char cCharToReplace, char szReplaceCharWith[])
{
	char * szNewString = (char*)malloc(MAXSTRING* (sizeof(char)));
	char *szBaseStringEnum = szBaseString;
	int iStrCharIndex = 0;
	int iStrLastIndex = 0;
	int iStrCurrIndex = 0;
	int iCharsToCopy = 0;
	ZeroMemory((void*)szNewString, MAXSTRING);
	
	while (strcmp(szBaseStringEnum, "\0") != 0 )
	{
		iStrCurrIndex = (int)szBaseStringEnum;
		iStrCharIndex = (int)strchr(szBaseStringEnum, cCharToReplace);
		if (iStrCharIndex != NULL)
		{
			iCharsToCopy = iStrCharIndex - iStrCurrIndex;
			strncat_s(szNewString, MAXSTRING, szBaseStringEnum, iCharsToCopy);
			strcat_s(szNewString, MAXSTRING, szReplaceCharWith);
			szBaseStringEnum = szBaseStringEnum + (iStrCharIndex - iStrCurrIndex) + 1;
		}
		else
		{
			strcat_s(szNewString, MAXSTRING, szBaseStringEnum);
			ZeroMemory(szBaseStringEnum, strlen(szBaseStringEnum));
		}
	}
	return szNewString;
}


int json_compliant_convert(char * szToComply)
{
	char cJSONEscapeChars[]= { '/', '\\' };
	int iNumEscapeJSONChars = 2;
	int iLoop;
	char szReplaceWith[MAXSTRING];
	char *szComplaintString;

	ZeroMemory(szReplaceWith, MAXSTRING);

	szReplaceWith[0] = '\\';
	for (iLoop = 0; iLoop < iNumEscapeJSONChars; iLoop++ )
	{		
		szReplaceWith[1] = cJSONEscapeChars[iLoop];
		szComplaintString = replace_string(szToComply, cJSONEscapeChars[iLoop], szReplaceWith);
		strcpy_s(szToComply, MAXSTRING, szComplaintString);
		free(szComplaintString);
	}
	return 0;
}

int export_report_to_json(char * szJSONFileName)
{
	int iLoop;
	RPC_INFO *prpc_info;

	char szJSONCompliantString[MAXSTRING];
	char szHostname[MAXSTRING];
	
	gethostname(szHostname,MAXSTRING);
	struct hostent *remoteHost = gethostbyname(szHostname);
	struct in_addr addr;

	if (iVerbose)
		printf("\tCreating JSON output\n");
	FILE *JSONFile;	

	fopen_s(&JSONFile, szJSONFileName, "w");
	fprintf(JSONFile, "{\n");
	fprintf(JSONFile, "\"hostname\": \"%s\",\n", szHostname);

	int iAddressCount = 0;
	if (remoteHost->h_addrtype == AF_INET)
	{
		fprintf(JSONFile, "\"Addresses\": [ \n");

		while (remoteHost->h_addr_list[iAddressCount] != 0) {
			addr.s_addr = *(u_long *)remoteHost->h_addr_list[iAddressCount++];
			if (remoteHost->h_addr_list[iAddressCount] == 0)
				fprintf(JSONFile, "{ \"IP Address\": \"%s\"}\n", inet_ntoa(addr));
			else
				fprintf(JSONFile, "{ \"IP Address\": \"%s\"},\n", inet_ntoa(addr));
		}
		fprintf(JSONFile, "],\n");
	}
	
	fprintf(JSONFile, "\"rpc-info\": [\n");
	for (iLoop = 0; iLoop < irpc_info_database_count; iLoop++)
	{
		prpc_info = &rpc_info_database[iLoop];
		fprintf(JSONFile, "\t{\n");
		fprintf(JSONFile, "\t\t\"ID\" : \"%s\",\n", prpc_info->szID);
		fprintf(JSONFile, "\t\t\"Version\" : \"%d.%d\",\n", prpc_info->iMajorVersion, prpc_info->iMinorVersion);

		strcpy_s(szJSONCompliantString, MAXSTRING, prpc_info->szAnnotation);
		json_compliant_convert(szJSONCompliantString);

		fprintf(JSONFile, "\t\t\"Annotation\" : \"%s\",\n", szJSONCompliantString);
		
		fprintf(JSONFile, "\t\t\"Protocol\" : \"%s\",\n", prpc_info->szProtocol);	
		
		strcpy_s(szJSONCompliantString, MAXSTRING, prpc_info->szServiceEndpoint);
		json_compliant_convert(szJSONCompliantString);
		fprintf(JSONFile, "\t\t\"ServiceEndpoint\" : \"%s\",\n", szJSONCompliantString);
		fprintf(JSONFile, "\t\t\"UUID\" : \"%s\",\n", prpc_info->szUUID);
		fprintf(JSONFile, "\t\t\"ReachableStatus\" : \"%d\"\n", prpc_info->iStatus);
		fprintf(JSONFile, "\t}");
		if (iLoop == (irpc_info_database_count - 1))
		{
			fprintf(JSONFile, "\n");
		}
		else
		{
			fprintf(JSONFile, ",\n");
		}
	}
	fprintf(JSONFile, "]\n");
	fprintf(JSONFile, "}\n");

	return 0;
}


int connect_to_host(const char *remote_host, const char* szPort)
{
	SOCKET s = INVALID_SOCKET;
	int iResult;
	struct addrinfo *result;
	WORD wVersionRequested;
	WSADATA wsaData;
	int iWSALastError = 0;

	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);

	iResult = getaddrinfo((PCSTR)remote_host, (PCSTR)szPort, NULL, &result);
	if (iResult != 0) {
		iWSALastError = WSAGetLastError();
	}
	else
	{
		s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		iResult = connect(s, result->ai_addr, (int)result->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			iWSALastError = WSAGetLastError();
		}
	}
	freeaddrinfo(result);
	closesocket(s);
	WSACleanup();
	return iWSALastError;
}

int rpc_uuid_in_target_list(char *szIDlist, char *szID)
{
	if (_strcmpi(szIDlist, "all") == 0)
		return 1;
	else
	{
		_strlwr_s(szIDlist, MAXSTRING);
		return (int)strstr(szIDlist,szID);
	}
}

int test_connection_to_rpc_port(char szUUIDList[])
{
	int iLoop;
	int iStatus;	 
	
	if (iVerbose)
		printf("Testing conection to tcp based rpc services\n");
	for (iLoop = 0; iLoop < irpc_info_database_count; iLoop++)
	{
		if ((strcmp(rpc_info_database[iLoop].szProtocol, "ncacn_ip_tcp") == 0)
			&&(rpc_uuid_in_target_list(szUUIDList, rpc_info_database[iLoop].szID) != NULL))
		{
			if (iVerbose)
			{
				printf("\tAttempting to connect to service %s on port %s", rpc_info_database[iLoop].szID,
					rpc_info_database[iLoop].szServiceEndpoint);
			}
			iStatus = connect_to_host(rpc_info_database[iLoop].szNetworkAddress,
				rpc_info_database[iLoop].szServiceEndpoint);
			rpc_info_database[iLoop].iStatus = iStatus;
			if (iVerbose)
			{
				if (iStatus == 0)
					printf("..  connect success\n");
				else
					printf(".. connect failed with error %d\n", iStatus);
			}
		}
	}
	return 0;
}

int get_rpc_interfaces_by_host(char *remote_host)
{
	
	RPC_CSTR a_protocol = (RPC_CSTR)"ncacn_ip_tcp";
	RPC_CSTR a_UUID = NULL;
	RPC_CSTR a_netadr = (RPC_CSTR)remote_host;
	RPC_CSTR a_endpoint = NULL;
	RPC_CSTR a_options = NULL;

	RPC_STATUS rpc_status;
	RPC_CSTR binding;
	RPC_CSTR szBuff[MAXSTRING];
	RPC_BINDING_HANDLE handle, handle2;
	RPC_IF_ID id;

	UUID uuid;
	RPC_EP_INQ_HANDLE context;
	ULONG l;

	RPC_CSTR rzAnnotation;
	RPC_CSTR rzString;

	RPC_CSTR rzProtocol;
	RPC_CSTR rzNetworkAddress;
	RPC_CSTR rzEndpoint;

	char szServiceEndpoint[MAXSTRING];
	char szProtocol[MAXSTRING];
	char szNetworkAddress[MAXSTRING];
	char szID[MAXSTRING];
	char szUUID[MAXSTRING];
	int iMajorVersion;
	int iMinorVersion;
	char szAnnotation[MAXSTRING];
	
	rpc_status = RpcStringBindingCompose(a_UUID, a_protocol, a_netadr, a_endpoint, a_options, &binding);	
	if (rpc_status != RPC_S_OK)
	{
		DceErrorInqText(rpc_status, (RPC_CSTR)szBuff);
		printf("Unable to compose binding string: Error %d:%s", rpc_status, (char *)szBuff);
		return 0;
	}
	if (iVerbose)
	{
		printf("Initial RPC Binding String: %s\n", binding);
	}
	rpc_status = RpcBindingFromStringBinding(binding, &handle);	 
	if (rpc_status != RPC_S_OK)
	{
		DceErrorInqText(rpc_status, (RPC_CSTR)szBuff);
		printf("Unable to create binding handle: %d:%s", rpc_status, (char *)szBuff);
		RpcStringFree(&binding);
		return 0;
	}
	l = RPC_C_EP_ALL_ELTS;
	rpc_status = RpcMgmtEpEltInqBegin(handle, l, NULL, 0, &uuid, &context);
	if (rpc_status != RPC_S_OK)
	{
		DceErrorInqText(rpc_status, (RPC_CSTR)szBuff);
		printf("Unable to initiate endplont mappy query, erro %d:%s", rpc_status, (char *)szBuff);
		return 0;
	}
	else
	{
		if (iVerbose)
			printf("Attempting to query RPC Endpoint Mapper Database\n");	
		while (RpcMgmtEpEltInqNext(context, &id, &handle2, &uuid, &rzAnnotation) == RPC_S_OK)
		{
			UuidToString(&id.Uuid, &rzString);
			strcpy_s(szID, MAXSTRING, (char *)rzString);
			iMajorVersion = id.VersMajor;
			iMinorVersion = id.VersMinor;
			RpcStringFree(&rzString);
			RpcBindingToStringBinding(handle2, &rzString);			
			RpcStringBindingParse(rzString, NULL, &rzProtocol, &rzNetworkAddress, &rzEndpoint, NULL);
			strcpy_s(szProtocol, MAXSTRING, (char *)rzProtocol);
			strcpy_s(szNetworkAddress, MAXSTRING, (char *)rzNetworkAddress);
			strcpy_s(szServiceEndpoint, MAXSTRING, (char *)rzEndpoint);
			RpcStringFree(&rzString);			
			if (!UuidIsNil(&uuid, &rpc_status))
			{
				UuidToString(&uuid, &rzString);
				strcpy_s(szUUID, MAXSTRING, (char *)rzString);
				RpcStringFree(&rzString);
			}
			strcpy_s(szAnnotation, MAXSTRING, (char *)rzAnnotation);			
			strcpy_s(rpc_info_database[irpc_info_database_count].szProtocol, MAXSTRING,szProtocol);
			strcpy_s(rpc_info_database[irpc_info_database_count].szNetworkAddress, MAXSTRING, szNetworkAddress);
			strcpy_s(rpc_info_database[irpc_info_database_count].szID, MAXSTRING, szID);
			strcpy_s(rpc_info_database[irpc_info_database_count].szUUID, MAXSTRING, szUUID);
			strcpy_s(rpc_info_database[irpc_info_database_count].szAnnotation, MAXSTRING, szAnnotation);
			rpc_info_database[irpc_info_database_count].iMajorVersion = iMajorVersion;
			rpc_info_database[irpc_info_database_count].iMinorVersion = iMinorVersion;
			strcpy_s(rpc_info_database[irpc_info_database_count].szServiceEndpoint, MAXSTRING, szServiceEndpoint);
			irpc_info_database_count++;
			if (iVerbose)
			{
				printf("\tFound Service ID: %s\n" , szID);
				printf("\t\tVersion: %d.%d\n", iMajorVersion, iMinorVersion);
				printf("\t\tNetwork Address: %s\n", szNetworkAddress);
				printf("\t\tAnnotation: %s\n", szAnnotation);
				printf("\t\tProtocol: %s\n", szProtocol);
				printf("\t\tUUID: %s\n", szUUID);
				printf("\t\tEndpoint: %s\n", szServiceEndpoint);
			}
			if (handle2 != NULL)
				RpcBindingFree(&handle2);
			if (rzAnnotation != NULL)
				RpcStringFree(&rzAnnotation);
			RpcStringFree(&rzProtocol);
			RpcStringFree(&rzNetworkAddress);
			RpcStringFree(&rzEndpoint);
		}		
		rpc_status = GetLastError();
		if (rpc_status != RPC_S_OK)
		{
			DceErrorInqText(rpc_status,(RPC_CSTR)szBuff);
			printf("Failed to enumerate RPC Services. Error %d:%s\n", rpc_status, (char *) szBuff);
			return 0;
		}
		RpcMgmtEpEltInqDone(&context);

		if (iVerbose)
			printf("%d services registered in the database\n", irpc_info_database_count);
		if (irpc_info_database_count == 0)
			printf("No services registered in the endpoint mapper. Is your host down?\n");
	}
	return 0;
}

int print_usage(char *argv[])
{
	printf("Usage:\n");
	printf("%s -h host [-s service_ID_list] [-j filename.json] [-v]\n", argv[0]);
	printf("-h host\t\t\t:name or IP Address of the remote host to query\n");
	printf("-s service_ID_list\t:list of RPC IDs to test tcp connect\n");
	printf("-j filename.json\t:export results to filename.json\n");
	printf("-v\t\t\t:verbose output\n");

	printf("\n\n");

	printf("Example:\n");
	printf("%s -h dc1 -s \"12345678-1234-ABCD-EF00-01234567CFFB,12345778-1234-ABCD-EF00-0123456789AB,E3514235-4B06-11D1-AB04-00C04FC2DCD2\" -j filename.json -v\n", argv[0]);


	return 0;
}

int main(int argc, char *argv[])
{ 
	int iLoop;
	char szTargetHost[MAXSTRING];
	char szJSONFileName[MAXSTRING];
	char szUUIDList[MAXSTRING];

	ZeroMemory(szTargetHost, MAXSTRING);
	ZeroMemory(szJSONFileName, MAXSTRING);
	ZeroMemory(szUUIDList, MAXSTRING);
	
	BOOL bGotHost = FALSE;
	BOOL bGotJSONFileName = FALSE;
	BOOL bGotUUIDList = FALSE;
	
	irpc_info_database_count = 0;
	

	for (iLoop = 0; iLoop < argc; iLoop++)
	{
		if (((strcmp(argv[iLoop], "-s") == 0) && (argc >(iLoop + 1))))
		{
			strcpy_s(szUUIDList, MAXSTRING, argv[iLoop + 1]);
			bGotUUIDList = TRUE;
		}

		if (((strcmp(argv[iLoop], "-h") == 0)&& (argc >(iLoop+1))))
		{	
			strcpy_s(szTargetHost, MAXSTRING, argv[iLoop + 1]);
			bGotHost = TRUE;
		}
		if (((strcmp(argv[iLoop], "-j") == 0) && (argc > (iLoop + 1))))
		{
			strcpy_s(szJSONFileName, MAXSTRING, argv[iLoop + 1]);
			bGotJSONFileName = TRUE;
		}
		if ((strcmp(argv[iLoop], "-v") == 0) )
		{
			iVerbose = 1;
		}
	}

	if (bGotHost)
	{
		get_rpc_interfaces_by_host(szTargetHost);
		if (bGotJSONFileName)
		{
			test_connection_to_rpc_port(szUUIDList);
			export_report_to_json(szJSONFileName);
		}
	}
	else
	{
		print_usage(argv);
	}	
	return 0;
}
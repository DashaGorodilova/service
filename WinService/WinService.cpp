#include <Windows.h>
#include <iostream>
#include <fstream>
#include <WTSApi32.h>
#include <sstream>
using namespace std;

#define SERVICE_NAME TEXT("DashaServesIB")
SERVICE_STATUS				ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE		hServiceStatusHandler = NULL;
HANDLE						hServiceEvent = NULL;

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpArgv);
DWORD WINAPI ServiceControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID dwEventData, LPVOID dwContent);
void ServiceReportStatus(
	DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint);
void ServiceInit(DWORD dwArgc, LPTSTR* lpArgv);
void ServiceInstall(void);
void ServiceDelete(void);
void ServiceStart(void);
void ServiceStop(void);

int main(int argc, CHAR* argv[])
{
	cout << "In main fun Start" << endl;

	BOOL bStServiceCtrlDispatcher = FALSE;

	if (lstrcmpiA(argv[1], "install") == 0)
	{
		ServiceInstall();
		cout << "Installation Success" << endl;

	}
	else if (lstrcmpiA(argv[1], "start") == 0)
	{
		ServiceStart();
		cout << "ServiceStart Success" << endl;

	}
	else if (lstrcmpiA(argv[1], "stop") == 0)
	{
		ServiceStop();
		cout << "ServiceStop Success" << endl;
	}
	else if (lstrcmpiA(argv[1], "delete") == 0)
	{
		ServiceDelete();
		cout << "ServiceDelete" << endl;
	}

	else
	{
		SERVICE_TABLE_ENTRY DispatchTable[] =
		{
			{const_cast<LPWSTR>(SERVICE_NAME), reinterpret_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain)},
			{NULL,NULL}
		};

		bStServiceCtrlDispatcher = StartServiceCtrlDispatcher(DispatchTable);
		if (FALSE == bStServiceCtrlDispatcher)
		{
			cout << "StartServiceCtrlDispatcher Failed" << GetLastError() << endl;
		}
		else
		{
			cout << "StartServiceCtrlDispatcher Success" << endl;
		}

	}

	cout << "In main fun End" << endl;
	system("PAUSE");
	return 0;
}

void StartUiProcessInSession(DWORD wtsSession) {	
	std::wstring sessionPrefix = L"WinSta0\\Default";
	std::wstring sessionName = sessionPrefix + std::to_wstring(wtsSession);

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcess(
		L"C:\\Windows\\System32\\notepad.exe", 
		NULL,                                   
		NULL,                                   
		NULL,                                  
		FALSE,                                  
		NORMAL_PRIORITY_CLASS,                  
		NULL,                                  
		NULL,                                  
		&si,                                    
		&pi)) {                                 
		std::cerr << "Failed to create UI process for session " << wtsSession << std::endl;
		return;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}


void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpArgv)
{
	cout << "ServiceMain Start" << endl;
	BOOL bServiceStatus = FALSE;

	hServiceStatusHandler = RegisterServiceCtrlHandlerEx(
		SERVICE_NAME,(LPHANDLER_FUNCTION_EX)ServiceControlHandler,
		NULL);

	if (NULL == hServiceStatusHandler)
	{
		cout << "RegisterServiceCtrlHandlerEx Failed" << GetLastError() << endl;
	}
	else
	{
		cout << "RegisterServiceCtrlHandlerEx Success" << endl;
	}

	ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN || SERVICE_ACCEPT_STOP || SERVICE_ACCEPT_SESSIONCHANGE;
	ServiceStatus.dwServiceSpecificExitCode = 0;

	ServiceReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	bServiceStatus = SetServiceStatus(hServiceStatusHandler, &ServiceStatus);
	if (FALSE == bServiceStatus)
	{
		cout << "Service Status initial Setup FAILED = " << GetLastError() << endl;
	}
	else
	{
		cout << "Service Status initial Setup SUCCESS" << endl;
	}

	//being told
	PWTS_SESSION_INFO wtsSessions;
	DWORD sessionCount;
	if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &wtsSessions, &sessionCount))
	{
		return;
	}
	
	for (auto i = 1; i < sessionCount; ++i)
	{
		auto wtsSession = wtsSessions[i].SessionId;
		HANDLE userToken;
		WTSQueryUserToken(wtsSessions[i].SessionId, &userToken);
		PROCESS_INFORMATION	pi{};
		STARTUPINFO si;
		WCHAR path[] = L"C:\\Windows\\System32\\notepad.exe";
		CreateProcessAsUser(userToken, NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	}

	//being told

	ServiceInit(dwArgc, lpArgv);

	cout << "ServiceMain End" << endl;
}//..........Service Main...................//


//..........Service Control Handler................//
DWORD WINAPI ServiceControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContent) 
{
	DWORD errorCode = NO_ERROR;
	switch (dwControl)
	{
	case SERVICE_CONTROL_INTERROGATE:
		break;
	case SERVICE_CONTROL_STOP:
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		break;
	case SERVICE_CONTROL_SHUTDOWN:
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		break;
	case SERVICE_CONTROL_SESSIONCHANGE:
	{
		WTSSESSION_NOTIFICATION* sessionNotification = static_cast<WTSSESSION_NOTIFICATION*>(lpEventData);
		if (dwEventType == WTS_SESSION_LOGON || dwEventType == WTS_SESSION_CREATE || dwEventType == WTS_REMOTE_CONNECT) {
			StartUiProcessInSession(sessionNotification->dwSessionId);
		}
	}
	default:
		errorCode = ERROR_CALL_NOT_IMPLEMENTED;
		break;
	}
	ServiceReportStatus(ServiceStatus.dwCurrentState, errorCode, 0);
	return errorCode;
}

//..........Service Init.............................//
void ServiceInit(DWORD dwArgc, LPTSTR* lpArgv)
{
	cout << "ServiceInit Start" << endl;

	hServiceEvent = CreateEvent(
		NULL, //security attribute
		TRUE, //manual reset event
		FALSE,//Non Signaled
		NULL);//name of event

	if (NULL == hServiceEvent)
	{
		ServiceReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
	}
	else
	{
		ServiceReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
	}


	while (1)
	{
		WaitForSingleObject(hServiceEvent, INFINITE);
		ServiceReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
	}

	cout << "ServiceInit End" << endl;
}//..........Service Init.............................//


//..........ServiceReportStatus.............................//
void ServiceReportStatus(
	DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHunt)
{
	cout << "ServiceReportStatus Start" << endl;

	static DWORD dwCheckPoint = 1;
	BOOL bSetServiceStatus = FALSE;

	ServiceStatus.dwCurrentState = dwCurrentState;
	ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
	ServiceStatus.dwWaitHint = dwWaitHunt;

	if (dwCurrentState == SERVICE_START_PENDING)
	{
		ServiceStatus.dwControlsAccepted = 0;
	}
	else
	{
		ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN || SERVICE_ACCEPT_STOP || SERVICE_ACCEPT_SESSIONCHANGE;
	}

	if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
	{
		ServiceStatus.dwCheckPoint = 0;
	}
	else
	{
		ServiceStatus.dwCheckPoint = dwCheckPoint++;
	}

	bSetServiceStatus = SetServiceStatus(hServiceStatusHandler, &ServiceStatus);

	if (FALSE == bSetServiceStatus)
	{
		cout << "Service Status Failed = " << GetLastError() << endl;
	}
	else
	{
		cout << "Service Status SUCCESS" << endl;
	}

	cout << "ServiceReportStatus END" << endl;
} //..........ServiceReportStatus.............................//



void ServiceInstall(void)
{
	cout << "ServiceInstall Start" << endl;

	SC_HANDLE		hScOpenSCManager = NULL;
	SC_HANDLE		hScCreateService = NULL;
	DWORD			dwGetModuleFileName = 0;
	TCHAR			szPath[MAX_PATH];

	dwGetModuleFileName = GetModuleFileName(NULL, szPath, MAX_PATH);
	if (0 == dwGetModuleFileName)
	{
		cout << "Service Installation Failed" << GetLastError() << endl;
	}
	else
	{
		cout << "Successfully install the file\n" << endl;
	}

	hScOpenSCManager = OpenSCManager(
		NULL,
		NULL,
		SC_MANAGER_ALL_ACCESS);

	if (NULL == hScOpenSCManager)
	{
		cout << "OpenSCManager Failed = " << GetLastError() << endl;
	}
	else
	{
		cout << "OpenSCManager Success" << endl;
	}

	hScCreateService = CreateService(
		hScOpenSCManager,
		SERVICE_NAME,
		SERVICE_NAME,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		szPath,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL);

	if (NULL == hScCreateService)
	{
		cout << "CreateSeervice Failed = " << GetLastError() << endl;
		CloseServiceHandle(hScOpenSCManager);
	}
	else
	{
		cout << "CreateService Success" << endl;
	}

	CloseServiceHandle(hScCreateService);
	CloseServiceHandle(hScOpenSCManager);

	cout << "ServiceInstall End" << endl;
}


void ServiceDelete(void)
{
	cout << "ServiceDelete Start" << endl;

	SC_HANDLE		hScOpenSCManager = NULL;
	SC_HANDLE		hScOpenService = NULL;
	BOOL			bDeleteService = FALSE;

	hScOpenSCManager = OpenSCManager(
		NULL,
		NULL,
		SC_MANAGER_ALL_ACCESS);

	if (NULL == hScOpenSCManager)
	{
		cout << "OpenSCManager Failed = " << GetLastError() << endl;
	}
	else
	{
		cout << "OpenSCManager Success" << endl;
	}

	hScOpenService = OpenService(
		hScOpenSCManager,
		SERVICE_NAME,
		SERVICE_ALL_ACCESS);

	if (NULL == hScOpenService)
	{
		cout << "OpenService Failed = " << GetLastError() << endl;
	}
	else
	{
		cout << "OpenService Success " << endl;
	}

	bDeleteService = DeleteService(hScOpenService);

	if (FALSE == bDeleteService)
	{
		cout << "Delete Service Failed = " << GetLastError() << endl;
	}
	else
	{
		cout << "Delete Service Success" << endl;
	}

	CloseServiceHandle(hScOpenService);
	CloseServiceHandle(hScOpenSCManager);
	cout << "ServiceDelete End" << endl;

}


void ServiceStart(void)
{
	cout << "Inside ServiceStart function" << endl;

	BOOL			bStartService = FALSE;
	SERVICE_STATUS_PROCESS		SvcStatusProcess;
	SC_HANDLE		hOpenSCManager = NULL;
	SC_HANDLE		hOpenService = NULL;
	BOOL			bQueryServiceStatus = FALSE;
	DWORD			dwBytesNeeded;

	hOpenSCManager = OpenSCManager(
		NULL,
		NULL,
		SC_MANAGER_ALL_ACCESS);

	if (NULL == hOpenSCManager)
	{
		cout << "hOpenSCManager Failed = " << GetLastError() << endl;
	}
	else
	{
		cout << "hOpenSCManager Success" << endl;
	}

	hOpenService = OpenService(
		hOpenSCManager,
		SERVICE_NAME,
		SC_MANAGER_ALL_ACCESS);

	if (NULL == hOpenService)
	{
		cout << "OpenServcie Failed = " << GetLastError() << endl;
		CloseServiceHandle(hOpenSCManager);
	}
	else
	{
		cout << "OpenServcie SUCCESS" << endl;
	}

	bQueryServiceStatus = QueryServiceStatusEx(
		hOpenService,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&SvcStatusProcess,
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded);

	if (FALSE == bQueryServiceStatus)
	{
		cout << "QueryService Failed = " << GetLastError() << endl;
		CloseServiceHandle(hOpenService);
		CloseServiceHandle(hOpenSCManager);
	}
	else
	{
		cout << "QueryService Success" << endl;
	}

	if ((SvcStatusProcess.dwCurrentState != SERVICE_STOPPED) &&
		(SvcStatusProcess.dwCurrentState != SERVICE_STOP_PENDING))
	{
		cout << "service is already running" << endl;
	}
	else
	{
		cout << "Service is already stopped" << endl;
	}

	while (SvcStatusProcess.dwCurrentState == SERVICE_STOP_PENDING)
	{
		bQueryServiceStatus = QueryServiceStatusEx(
			hOpenService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&SvcStatusProcess,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded);

		if (FALSE == bQueryServiceStatus)
		{
			cout << "Query Service Failed = " << GetLastError() << endl;
			CloseServiceHandle(hOpenService);
			CloseServiceHandle(hOpenSCManager);
		}
		else
		{
			cout << "query Service Success" << endl;
		}
	}

	bStartService = StartService(
		hOpenService,
		NULL,
		NULL);

	if (FALSE == bStartService)
	{
		cout << "StartService Failed = " << GetLastError() << endl;
		CloseServiceHandle(hOpenService);
		CloseServiceHandle(hOpenSCManager);
	}
	else
	{
		cout << "StartService Success" << endl;
	}

	bQueryServiceStatus = QueryServiceStatusEx(
		hOpenService,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&SvcStatusProcess,
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded);

	if (FALSE == bQueryServiceStatus)
	{
		cout << "QueryService Failed = " << GetLastError() << endl;
		CloseServiceHandle(hOpenService);
		CloseServiceHandle(hOpenSCManager);
	}
	else
	{
		cout << "QueryService Success" << endl;
	}

	if (SvcStatusProcess.dwCurrentState == SERVICE_RUNNING)
	{
		cout << "Service Started Running..." << endl;
	}
	else
	{
		cout << "Service Running Failed  = " << GetLastError() << endl;
		CloseServiceHandle(hOpenService);
		CloseServiceHandle(hOpenSCManager);
	}

	CloseServiceHandle(hOpenService);
	CloseServiceHandle(hOpenSCManager);
	cout << "ServiceStart end" << endl;
}

void ServiceStop(void)
{
	cout << "Inside Service Stop" << endl;

	SERVICE_STATUS_PROCESS		SvcStatusProcess;
	SC_HANDLE					hScOpenSCManager = NULL;
	SC_HANDLE					hScOpenService = NULL;
	BOOL						bQueryServiceStatus = TRUE;
	BOOL						bControlService = TRUE;
	DWORD						dwBytesNeeded;

	//шаг 1
	hScOpenSCManager = OpenSCManager(
		NULL,
		NULL,
		SC_MANAGER_ALL_ACCESS);

	if (NULL == hScOpenSCManager)
	{
		cout << "OpenSCManager Failed = " << GetLastError() << endl;
	}
	else
	{
		cout << "OpenSCManager Success" << endl;
	}

	//шаг 2
	hScOpenService = OpenService(
		hScOpenSCManager,
		SERVICE_NAME,
		SC_MANAGER_ALL_ACCESS);

	if (NULL == hScOpenService)
	{
		cout << "OpenService Failed = " << GetLastError() << endl;
		CloseServiceHandle(hScOpenSCManager);
	}
	else
	{
		cout << "OpenService Success" << endl;
	}

	//шаг 3

	bQueryServiceStatus = QueryServiceStatusEx(
		hScOpenService,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&SvcStatusProcess,
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded);

	if (FALSE == bQueryServiceStatus)
	{
		cout << "QueryService Failed = " << GetLastError() << endl;
		CloseServiceHandle(hScOpenService);
		CloseServiceHandle(hScOpenSCManager);
	}
	else
	{
		cout << "QueryService Success" << endl;
	}

	//шаг 4
	bControlService = ControlService(
		hScOpenService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&SvcStatusProcess);

	if (TRUE == bControlService)
	{
		cout << "Control Service Success" << endl;
	}
	else
	{
		cout << "Control Service Failed = " << GetLastError() << endl;
		CloseServiceHandle(hScOpenService);
	}

	//шаг 5
	while (SvcStatusProcess.dwCurrentState != SERVICE_STOPPED)
	{
		bQueryServiceStatus = QueryServiceStatusEx(
			hScOpenService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&SvcStatusProcess,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded);

		if (TRUE == bQueryServiceStatus)
		{
			cout << "Queryservice FAiled = " << GetLastError() << endl;
			CloseServiceHandle(hScOpenService);
			CloseServiceHandle(hScOpenSCManager);
		}
		else
		{
			cout << "Query Service Success" << endl;
		}

		if (SvcStatusProcess.dwCurrentState == SERVICE_STOPPED)
		{
			cout << "Service stopped successfully" << endl;
			break;
		}
		else
		{
			cout << "Service stopped Faield = " << GetLastError() << endl;
			CloseServiceHandle(hScOpenService);
			CloseServiceHandle(hScOpenSCManager);
		}
	}

	CloseServiceHandle(hScOpenService);
	CloseServiceHandle(hScOpenSCManager);
	cout << "Service Stop" << endl;

}

/*
x86��x64��ע����Ϊx64��ϵͳ�����˽϶��Ȩ�޵�У�飬��Ҫ������Ȩ����
x64��Ȩ��Ҫ�����õ���ntdll.dll�е�δ����������RtlAdjustPrivilege().
*/
#include "stdafx.h"
#include "CreateRemoteThread.h"
#include <strsafe.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// Ψһ��Ӧ�ó������

CWinApp theApp;

using namespace std;

typedef enum  _WIN_VERSION
{
    WindowsNT,
    Windows2000,
    WindowsXP,
    Windows2003,
    WindowsVista,
    Windows7,
    Windows8,
    Windows10,
    WinUnknown
}WIN_VERSION;
typedef LONG  NTSTATUS;
typedef NTSTATUS(*fnRtlGetVersion )(PRTL_OSVERSIONINFOW lpVersionInformation);  

VOID InjectDll(ULONG_PTR ProcessID, WCHAR* strPath);
WIN_VERSION  GetWindowsVersion();
BOOL EnableDebugPrivilege();
BOOL InjectDllByRemoteThreadXP(const TCHAR* wzDllFile, ULONG_PTR ProcessId);
BOOL InjectDllByRemoteThreadWin7(const TCHAR* wzDllFile, ULONG_PTR ProcessId);

typedef long (__fastcall *pfnRtlAdjustPrivilege64)(ULONG,ULONG,ULONG,PVOID);
typedef long (__stdcall *pfnRtlAdjustPrivilege32)(ULONG,ULONG,ULONG,PVOID);

WIN_VERSION  WinVersion = WinUnknown;

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
    WinVersion = GetWindowsVersion();

	InjectDll(3296, L"testdll.dll");
    
    return 0;
}

VOID InjectDll(ULONG_PTR ProcessID, WCHAR* strPath)
{
    WCHAR wzPath[MAX_PATH] = {0};

    if (ProcessID == 0 || strPath == NULL)
    {
        printf("Inject Fail ProcessId or strPath is not exists \r\n");
        return;
    }

    GetCurrentDirectory(260,wzPath);
    wcsncat_s(wzPath, L"\\", 2);
    wcsncat_s(wzPath, strPath, wcslen(strPath));//dll����·��

    if (!PathFileExists(wzPath))
    {
        printf("Inject Fail strPath is not exists LastError [%d]\r\n", GetLastError());
        return;
    }    

    printf("Inject Target [%d], strPath [%S]\n", ProcessID, wzPath);

    if(WinVersion >= Windows7)
    {
        if (!InjectDllByRemoteThreadWin7(wzPath,ProcessID))
            printf("Inject Fail\r\n");
        else 
            printf ("Inject Success\r\n");
    }
    else
    {
        if (!InjectDllByRemoteThreadXP(wzPath,ProcessID))
            printf("Inject Fail\r\n");            
        else 
            printf("Inject Success\r\n");
    }
}


BOOL InjectDllByRemoteThreadWin7(const TCHAR* wzDllFile, ULONG_PTR ProcessId)
{
    if (NULL == wzDllFile || 0 == ::_tcslen(wzDllFile) || ProcessId == 0 || -1 == _taccess(wzDllFile, 0))
    {
        return FALSE;
    }
    HANDLE                 hProcess = NULL;
    HANDLE                 hThread  = NULL;
    DWORD                  dwRetVal    = 0;
    LPTHREAD_START_ROUTINE FuncAddress = NULL;
    DWORD  dwSize = 0;
    TCHAR* VirtualAddress = NULL;
    //Ԥ���룬֧��Unicode
#ifdef _UNICODE
    FuncAddress = (PTHREAD_START_ROUTINE)::GetProcAddress(::GetModuleHandle(_T("Kernel32")), "LoadLibraryW");
#else
    FuncAddress = (PTHREAD_START_ROUTINE)::GetProcAddress(::GetModuleHandle(_T("Kernel32")), "LoadLibraryA");
#endif

    if (FuncAddress==NULL)
    {
        return FALSE;
    }

#ifdef _WIN64
    pfnRtlAdjustPrivilege64 RtlAdjustPrivilege = NULL;
	HMODULE temp = LoadLibraryW(L"ntdll.dll");
    RtlAdjustPrivilege=(pfnRtlAdjustPrivilege64)GetProcAddress(temp,"RtlAdjustPrivilege");
#else
    pfnRtlAdjustPrivilege32 RtlAdjustPrivilege = NULL;
    RtlAdjustPrivilege=(pfnRtlAdjustPrivilege32)GetProcAddress((HMODULE)(FuncAddress(L"ntdll.dll")),"RtlAdjustPrivilege");
#endif

    if (RtlAdjustPrivilege==NULL)
    {
        return FALSE;
    }
        /*
        .���� SE_BACKUP_PRIVILEGE, "17", ����
        .���� SE_RESTORE_PRIVILEGE, "18", ����
        .���� SE_SHUTDOWN_PRIVILEGE, "19", ����
        .���� SE_DEBUG_PRIVILEGE, "20", ����
        */
    RtlAdjustPrivilege(20,1,0,&dwRetVal);  //19

    hProcess = OpenProcess(PROCESS_ALL_ACCESS,FALSE, ProcessId);

    if (NULL == hProcess)
    {
        printf("Open Process Fail lastError [%d]\r\n", GetLastError());
        return FALSE;
    }

    // ��Ŀ������з����ڴ�ռ�
    dwSize = (DWORD)::_tcslen(wzDllFile) + 1;
    VirtualAddress = (TCHAR*)::VirtualAllocEx(hProcess, NULL, dwSize * sizeof(TCHAR), MEM_COMMIT, PAGE_READWRITE);  
    if (NULL == VirtualAddress)
    {
        printf("Virtual Process Memory Fail lastError [%d]\r\n", GetLastError());
        CloseHandle(hProcess);
        return FALSE;
    }

    // ��Ŀ����̵��ڴ�ռ���д���������(ģ����)
    if (FALSE == ::WriteProcessMemory(hProcess, VirtualAddress, (LPVOID)wzDllFile, dwSize * sizeof(TCHAR), NULL))
    {
        printf("Write Data Fail LastError [%d]\r\n", GetLastError());
        VirtualFreeEx(hProcess, VirtualAddress, dwSize, MEM_DECOMMIT);
        CloseHandle(hProcess);
        return FALSE;
    }

    hThread = ::CreateRemoteThread(hProcess, NULL, 0, FuncAddress, VirtualAddress, 0, NULL);
    if (NULL == hThread)
    {
        printf("CreateRemoteThread Fail lastError [%d]\r\n", GetLastError());
        VirtualFreeEx(hProcess, VirtualAddress, dwSize, MEM_DECOMMIT);
        CloseHandle(hProcess);
        return FALSE;
    }
    // �ȴ�Զ���߳̽���
    WaitForSingleObject(hThread, INFINITE);
    // ������Դ
    VirtualFreeEx(hProcess, VirtualAddress, dwSize, MEM_DECOMMIT);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return TRUE;
}


BOOL InjectDllByRemoteThreadXP(const TCHAR* wzDllFile, ULONG_PTR ProcessId)
{
    // ������Ч
    if (NULL == wzDllFile || 0 == ::_tcslen(wzDllFile) || ProcessId == 0 || -1 == _taccess(wzDllFile, 0))
    {    
        return FALSE;
    }
    HANDLE hProcess = NULL;
    HANDLE hThread  = NULL;
    DWORD dwSize = 0;
    TCHAR* VirtualAddress = NULL;
    LPTHREAD_START_ROUTINE FuncAddress = NULL;

    if(!EnableDebugPrivilege())
    {
        printf("EnableDebugPrivilege fail lasterror is [%d]\n", GetLastError());
        return FALSE;
    }

    // ��ȡĿ����̾��
    hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, ProcessId);
    if (NULL == hProcess)
    {
        printf("Open Process Fail LastError [%d]\r\n", GetLastError());
        return FALSE;
    }
    // ��Ŀ������з����ڴ�ռ�
    dwSize = (DWORD)::_tcslen(wzDllFile) + 1;
    VirtualAddress = (TCHAR*)::VirtualAllocEx(hProcess, NULL, dwSize * sizeof(TCHAR), MEM_COMMIT, PAGE_READWRITE);
    if (NULL == VirtualAddress)
    {
        printf("Virtual Process Memory Fail LastError [%d]\r\n", GetLastError());
        CloseHandle(hProcess);
        return FALSE;
    }
    // ��Ŀ����̵��ڴ�ռ���д���������(ģ����)
    if (FALSE == ::WriteProcessMemory(hProcess, VirtualAddress, (LPVOID)wzDllFile, dwSize * sizeof(TCHAR), NULL))
    {
        printf("Write Data Fail LastError [%d]\r\n", GetLastError());
        VirtualFreeEx(hProcess, VirtualAddress, dwSize, MEM_DECOMMIT);
        CloseHandle(hProcess);
        return FALSE;
    }
    // �� Kernel32.dll �л�ȡ LoadLibrary ������ַ
#ifdef _UNICODE
    FuncAddress = (PTHREAD_START_ROUTINE)::GetProcAddress(::GetModuleHandle(_T("Kernel32")), "LoadLibraryW");
#else
    FuncAddress = (PTHREAD_START_ROUTINE)::GetProcAddress(::GetModuleHandle(_T("Kernel32")), "LoadLibraryA");
#endif

    if (NULL == FuncAddress)
    {
        printf("Get LoadLibrary Fail LastError [%d]\r\n", GetLastError());
        VirtualFreeEx(hProcess, VirtualAddress, dwSize, MEM_DECOMMIT);
        CloseHandle(hProcess);
        return false;
    }

    // ����Զ���̵߳��� LoadLibrary
    hThread = ::CreateRemoteThread(hProcess, NULL, 0, FuncAddress, VirtualAddress, 0, NULL);
    if (NULL == hThread)
    {
        printf("CreateRemoteThread Fail LastError [%d]\r\n", GetLastError());
        VirtualFreeEx(hProcess, VirtualAddress, dwSize, MEM_DECOMMIT);
        CloseHandle(hProcess);
        return FALSE;
    }

    // �ȴ�Զ���߳̽���
    WaitForSingleObject(hThread, INFINITE);
    // ����
    VirtualFreeEx(hProcess, VirtualAddress, dwSize, MEM_DECOMMIT);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return TRUE;
}

WIN_VERSION  GetWindowsVersion()
{
    RTL_OSVERSIONINFOEXW verInfo = { 0 };  
    verInfo.dwOSVersionInfoSize = sizeof( verInfo );  

    fnRtlGetVersion RtlGetVersion = (fnRtlGetVersion)GetProcAddress( GetModuleHandleW( L"ntdll.dll" ), "RtlGetVersion" );  
    if(RtlGetVersion != NULL && RtlGetVersion((PRTL_OSVERSIONINFOW)&verInfo) == 0)
    {
        if (verInfo.dwMajorVersion <= 4 )
        {
            return WindowsNT;
        }
        if (verInfo.dwMajorVersion == 5 && verInfo.dwMinorVersion == 0)
        {
            return Windows2000;
        }

        if (verInfo.dwMajorVersion == 5 && verInfo.dwMinorVersion == 1)
        {
            return WindowsXP;
        }
        if (verInfo.dwMajorVersion == 5 && verInfo.dwMinorVersion == 2)
        {
            return Windows2003;
        }
        if (verInfo.dwMajorVersion == 6 && verInfo.dwMinorVersion == 0)
        {
            return WindowsVista;
        }

        if (verInfo.dwMajorVersion == 6 && verInfo.dwMinorVersion == 1)
        {
            return Windows7;
        }
        if (verInfo.dwMajorVersion == 6 && verInfo.dwMinorVersion == 2 )
        {
            return Windows8;
        }
        if (verInfo.dwMajorVersion == 10 && verInfo.dwMinorVersion == 0 && verInfo.dwBuildNumber >= 10240)
        {
            return Windows10;
        }
    }

    return WinUnknown;
}

BOOL EnableDebugPrivilege()
{
    HANDLE hToken;   
    TOKEN_PRIVILEGES TokenPrivilege;
    LUID uID;
    if (!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken))
    {
        printf("OpenProcessToken is Error\n");
        return FALSE;
    }
    if (!LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&uID))
    {
        printf("LookupPrivilegeValue is Error\n");
        return FALSE;
    }
    TokenPrivilege.PrivilegeCount = 1;
    TokenPrivilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    TokenPrivilege.Privileges[0].Luid = uID;
    //���������ǽ��е���Ȩ��
    if (!AdjustTokenPrivileges(hToken,false,&TokenPrivilege,sizeof(TOKEN_PRIVILEGES),NULL,NULL))
    {
        printf("AdjuestTokenPrivileges is Error\n");
        return  FALSE;
    }
    return TRUE;
}

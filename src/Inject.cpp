#include <windows.h>
#include <cstdio>
#include <string>
int wmain(int argc,wchar_t** argv){
 if(argc!=3){fwprintf(stderr,L"Usage: Inject.exe PID DLL\n");return 2;}
 wchar_t full[32768];if(!GetFullPathNameW(argv[2],32768,full,nullptr)||GetFileAttributesW(full)==INVALID_FILE_ATTRIBUTES)return 3;
 DWORD pid=wcstoul(argv[1],nullptr,10);printf("Target pid %lu\n",pid);
 HANDLE p=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
 if(!p){printf("OpenProcess error %lu\n",GetLastError());return 4;}
 size_t bytes=(wcslen(full)+1)*2;void* remote=VirtualAllocEx(p,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 SIZE_T n=0;if(!remote||!WriteProcessMemory(p,remote,full,bytes,&n)||n!=bytes){CloseHandle(p);return 5;}
 auto load=(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");
 HANDLE t=CreateRemoteThread(p,nullptr,0,load,remote,0,nullptr);if(!t){VirtualFreeEx(p,remote,0,MEM_RELEASE);CloseHandle(p);return 6;}
 DWORD wait=WaitForSingleObject(t,10000),rc=0;if(wait==WAIT_OBJECT_0){GetExitCodeThread(t,&rc);VirtualFreeEx(p,remote,0,MEM_RELEASE);}
 CloseHandle(t);CloseHandle(p);printf("LoadLibrary result=%lu wait=%lu\n",rc,wait);return wait==WAIT_OBJECT_0&&rc?0:7;
}

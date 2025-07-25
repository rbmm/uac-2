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

void WINAPI ep(void*)
{
	ULONG cch = 0;
	PWSTR psz = 0;
	HRESULT hr;
	PCWSTR pcsz = L"ExpandEnvironmentStringsW";
	while (cch = HR(hr, ExpandEnvironmentStringsW(L"%tmp%\\Un_A.exe", psz, cch)))
	{
		if (psz)
		{
			PVOID pv;
			ULONG cb;
			pcsz = L"Unzip";
			if (NOERROR == (hr = Unzip(f1_begin, RtlPointerToOffset(f1_begin, f1_end), &pv, &cb)))
			{
				pcsz = L"CreateFileW";
				if (HANDLE hFile = HR(hr, fixH(CreateFileW(psz, FILE_APPEND_DATA, 0, 0, CREATE_ALWAYS, 0, 0))))
				{
					pcsz = L"WriteFile";
					HR(hr, WriteFile(hFile, pv, cb, &cb, 0));
					NtClose(hFile);
				}
				LocalFree(pv);

				if (NOERROR == hr)
				{
					STARTUPINFOW si = { sizeof(si) };
					PROCESS_INFORMATION pi;
					pcsz = L"CreateProcessW";
					if (HR(hr, CreateProcessW(psz, 0, 0, 0, 0, 0, 0, 0, &si, &pi)))
					{
						NtClose(pi.hThread);
						NtClose(pi.hProcess);
					}
				}
			}

			break;
		}

		psz = (PWSTR)alloca(cch * sizeof(WCHAR));
	}
	
	ShowErrorBox(0, hr, pcsz);

	ExitProcess(hr);
}

_NT_END
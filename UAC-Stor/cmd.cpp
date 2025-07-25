#include "stdafx.h"

_NT_BEGIN

extern const SECURITY_QUALITY_OF_SERVICE sqos = {
	sizeof(sqos), SecurityImpersonation, SECURITY_DYNAMIC_TRACKING, FALSE
};

extern const OBJECT_ATTRIBUTES oa_sqos = { sizeof(oa_sqos), 0, 0, 0, 0, const_cast<SECURITY_QUALITY_OF_SERVICE*>(&sqos) };

extern const TOKEN_PRIVILEGES tp_Assign = { 1, { { { SE_ASSIGNPRIMARYTOKEN_PRIVILEGE }, SE_PRIVILEGE_ENABLED } } };

NTSTATUS GetToken(_In_ PVOID buf, _In_ const TOKEN_PRIVILEGES* RequiredSet, _Out_ HANDLE* phToken)
{
	NTSTATUS status;

	union {
		PVOID pv;
		PBYTE pb;
		PSYSTEM_PROCESS_INFORMATION pspi;
	};

	pv = buf;
	ULONG NextEntryOffset = 0;

	do
	{
		pb += NextEntryOffset;

		HANDLE hProcess, hToken, hNewToken;

		CLIENT_ID ClientId = { pspi->UniqueProcessId };

		if (ClientId.UniqueProcess)
		{
			if (0 <= NtOpenProcess(&hProcess, PROCESS_QUERY_LIMITED_INFORMATION,
				const_cast<POBJECT_ATTRIBUTES>(&oa_sqos), &ClientId))
			{
				status = NtOpenProcessToken(hProcess, TOKEN_DUPLICATE, &hToken);

				NtClose(hProcess);

				if (0 <= status)
				{
					status = NtDuplicateToken(hToken, TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
						const_cast<POBJECT_ATTRIBUTES>(&oa_sqos), FALSE, TokenImpersonation, &hNewToken);

					NtClose(hToken);

					if (0 <= status)
					{
						status = NtAdjustPrivilegesToken(hNewToken, FALSE, const_cast<PTOKEN_PRIVILEGES>(RequiredSet), 0, 0, 0);

						if (STATUS_SUCCESS == status)
						{
							*phToken = hNewToken;
							return STATUS_SUCCESS;
						}
						NtClose(hNewToken);
					}
				}
			}
		}

	} while (NextEntryOffset = pspi->NextEntryOffset);

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS GetToken(_In_ const TOKEN_PRIVILEGES* RequiredSet, _Out_ HANDLE* phToken)
{
	NTSTATUS status;

	ULONG cb = 0x40000;

	do
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (PBYTE buf = new BYTE[cb += PAGE_SIZE])
		{
			if (0 <= (status = NtQuerySystemInformation(SystemProcessInformation, buf, cb, &cb)))
			{
				if (STATUS_INFO_LENGTH_MISMATCH == (status = GetToken(buf, RequiredSet, phToken)))
				{
					status = STATUS_UNSUCCESSFUL;
				}
			}

			delete[] buf;
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	return status;
}

NTSTATUS SetToken(HANDLE hToken)
{
	return NtSetInformationThread(NtCurrentThread(), ThreadImpersonationToken, &hToken, sizeof(hToken));
}

void StartCmd(_In_ PCWSTR lpTitle)
{
	LONG SessionId = WTSGetActiveConsoleSessionId();
	if (0 < SessionId)
	{
		WCHAR cmd[MAX_PATH];

		if (GetEnvironmentVariableW(L"ComSpec", cmd, _countof(cmd)))
		{
			NTSTATUS status;

			HANDLE hToken, hNewToken;
			if (0 <= GetToken(&tp_Assign, &hToken))
			{
				if (0 <= (status = SetToken(hToken)))
				{
					if (0 <= (status = NtDuplicateToken(hToken,
						TOKEN_ADJUST_SESSIONID | TOKEN_ADJUST_DEFAULT | TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY,
						0, FALSE, TokenPrimary, &hNewToken)))
					{
						if (0 <= (status = NtSetInformationToken(hNewToken, TokenSessionId, &SessionId, sizeof(SessionId))))
						{
							STARTUPINFOW si = {
								sizeof(si), 0, const_cast<PWSTR>(L"Winsta0\\Default"), const_cast<PWSTR>(lpTitle)
							};
							PROCESS_INFORMATION pi;
							if (CreateProcessAsUserW(hNewToken, cmd,
								const_cast<PWSTR>(L"* /k whoami /priv /groups"), 0, 0, FALSE, 0, 0, 0, &si, &pi))
							{
								NtClose(pi.hThread);
								NtClose(pi.hProcess);
							}
						}
						NtClose(hNewToken);
					}

					SetToken(0);
				}

				NtClose(hToken);
			}
		}
	}
}

_NT_END
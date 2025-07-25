#include "stdafx.h"

_NT_BEGIN

EXTERN_C
NTSYSAPI
NTSTATUS 
NTAPI 
NtGetNextThread(
				_In_ HANDLE ProcessHandle,
				_In_ HANDLE ThreadHandle,
				_In_ ACCESS_MASK DesiredAccess,
				_In_ ULONG HandleAttributes,
				_In_ ULONG Flags,
				_Out_ PHANDLE NewThreadHandle
				);

struct ThreadInfo : CONTEXT 
{
	HANDLE hThread = 0;
	ThreadInfo* next = 0;

	ThreadInfo()
	{
		RtlZeroMemory(static_cast<CONTEXT*>(this), sizeof(CONTEXT));
		ContextFlags = CONTEXT_CONTROL;
	}
};

void ResumeAndFree(_In_ ThreadInfo* next)
{
	if (ThreadInfo* pti = next)
	{
		do 
		{
			next = pti->next;

			if (HANDLE hThread = pti->hThread)
			{
				ZwResumeThread(hThread, 0);
				NtClose(hThread);
			}

			delete pti;

		} while (pti = next);
	}
}

NTSTATUS SuspendAll(_Out_ ThreadInfo** ppti)
{
	ThreadInfo* pti = 0;
	HANDLE ThreadHandle = 0, hThread;
	NTSTATUS status;
	BOOL bClose = FALSE;

	HANDLE UniqueThread = (HANDLE)GetCurrentThreadId();

loop:
	status = NtGetNextThread(NtCurrentProcess(), ThreadHandle, 
		THREAD_QUERY_LIMITED_INFORMATION|THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT, 
		0, 0, &hThread);

	if (bClose)
	{
		NtClose(ThreadHandle);
		bClose = FALSE;
	}

	if (0 <= status)
	{
		ThreadHandle = hThread;

		THREAD_BASIC_INFORMATION tbi;

		if (0 <= (status = ZwQueryInformationThread(hThread, ThreadBasicInformation, &tbi, sizeof(tbi), 0)))
		{
			if (tbi.ClientId.UniqueThread == UniqueThread)
			{
				bClose = TRUE;
				goto loop;
			}

			if (0 <= (status = ZwSuspendThread(hThread, 0)))
			{
				status = STATUS_NO_MEMORY;

				if (ThreadInfo* next = new ThreadInfo)
				{
					if (0 <= (status = ZwGetContextThread(hThread, next)))
					{
						next->next = pti;
						pti = next;
						next->hThread = hThread;
						goto loop;
					}

					delete next;
				}

				ZwResumeThread(hThread, 0);
			}
		}

		if (status == STATUS_THREAD_IS_TERMINATING)
		{
			bClose = TRUE;
			goto loop;
		}

		NtClose(hThread);
	}

	switch (status)
	{
	case STATUS_NO_MORE_ENTRIES:
	case STATUS_SUCCESS:
		*ppti = pti;
		return STATUS_SUCCESS;
	}

	ResumeAndFree(pti);

	*ppti = 0;
	return status;
}

typedef NTSTATUS
(NTAPI * MapViewOfSection)(
						   _In_ HANDLE SectionHandle,
						   _In_ HANDLE ProcessHandle,
						   _Outptr_result_bytebuffer_(*ViewSize) PVOID *BaseAddress,
						   _In_ ULONG_PTR ZeroBits,
						   _In_ SIZE_T CommitSize,
						   _Inout_opt_ PLARGE_INTEGER SectionOffset,
						   _Inout_ PSIZE_T ViewSize,
						   _In_ SECTION_INHERIT InheritDisposition,
						   _In_ ULONG AllocationType,
						   _In_ ULONG Win32Protect
						   );

void RemapSelf_I(PVOID ImageBase, HANDLE hSection, MapViewOfSection Map)
{
	if (0 <= ZwUnmapViewOfSection(NtCurrentProcess(), ImageBase))
	{
		SIZE_T ViewSize = 0;
		if (0 > Map(hSection, NtCurrentProcess(), &ImageBase, 0, 0, 0, &ViewSize, ViewUnmap, 0, PAGE_EXECUTE_READWRITE))
		{
			__debugbreak();
		}
	}
}

void RemapSelf()
{
	ThreadInfo* pti;

	if (0 <= SuspendAll(&pti))
	{
		if (PIMAGE_NT_HEADERS pinth = RtlImageNtHeader(&__ImageBase))
		{
			ULONG SizeOfImage = pinth->OptionalHeader.SizeOfImage;
			LARGE_INTEGER size = { SizeOfImage };
			HANDLE hSection;
			if (0 <= ZwCreateSection(&hSection, SECTION_ALL_ACCESS, 0, &size, PAGE_EXECUTE_READWRITE, SEC_COMMIT, 0))
			{
				PVOID BaseAddress = 0;
				SIZE_T ViewSize = 0;
				if (0 <= ZwMapViewOfSection(hSection, NtCurrentProcess(), &BaseAddress, 
					0, 0, 0, &ViewSize, ViewUnmap, 0, PAGE_EXECUTE_READWRITE))
				{
					memcpy(BaseAddress, &__ImageBase, SizeOfImage);

					reinterpret_cast<void (*)(PVOID , HANDLE , MapViewOfSection )>(
						RtlOffsetToPointer(BaseAddress, RtlPointerToOffset(&__ImageBase, RemapSelf_I)))
						(&__ImageBase, hSection, ZwMapViewOfSection);

					ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
				}

				NtClose(hSection);
			}
		}

		ResumeAndFree(pti);
	}
}

_NT_END
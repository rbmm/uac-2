
#include "stdafx.h"

void* __cdecl operator new[](size_t ByteSize)
{
	return HeapAlloc(GetProcessHeap(), 0, ByteSize);
}

void* __cdecl operator new(size_t ByteSize)
{
	return HeapAlloc(GetProcessHeap(), 0, ByteSize);
}

void __cdecl operator delete(void* Buffer)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}

void __cdecl operator delete(void* Buffer, size_t)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}

void __cdecl operator delete[](void* Buffer)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}

void __cdecl operator delete[](void* Buffer, size_t)
{
	HeapFree(GetProcessHeap(), 0, Buffer);
}

#ifdef _X86_

EXTERN_C_START

uintptr_t __security_cookie;

void __fastcall __security_check_cookie(__in uintptr_t _StackCookie)
{
	if (__security_cookie != _StackCookie)
	{
		__debugbreak();
	}
}

BOOL __cdecl _ValidateImageBase(PVOID pImageBase)
{
	return RtlImageNtHeader(pImageBase) != 0;
}

PIMAGE_SECTION_HEADER __cdecl _FindPESection(PVOID pImageBase, ULONG rva)
{
	return RtlImageRvaToSection(RtlImageNtHeader(pImageBase), pImageBase, rva);
}

#pragma comment(linker, "/include:@__security_check_cookie@4")
#pragma comment(linker, "/include:__ValidateImageBase")
#pragma comment(linker, "/include:__FindPESection")

EXTERN_C_END

#endif



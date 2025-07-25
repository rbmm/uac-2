

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */
/* at Tue Jan 19 05:14:07 2038
 */
/* Compiler settings for appinfo.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0628 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __appinfo_h_h__
#define __appinfo_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __LaunchAdminProcess_INTERFACE_DEFINED__
#define __LaunchAdminProcess_INTERFACE_DEFINED__

/* interface LaunchAdminProcess */
/* [version][uuid] */ 

typedef struct _MONITOR_POINT
    {
    long MonitorLeft;
    long MonitorRight;
    } 	MONITOR_POINT;

typedef struct _APP_STARTUP_INFO
    {
    wchar_t *lpszTitle;
    long dwX;
    long dwY;
    long dwXSize;
    long dwYSize;
    long dwXCountChars;
    long dwYCountChars;
    long dwFillAttribute;
    long dwFlags;
    short wShowWindow;
    struct _MONITOR_POINT MonitorPoint;
    } 	APP_STARTUP_INFO;

typedef struct _APP_PROCESS_INFORMATION
    {
    ULONG_PTR ProcessHandle;
    ULONG_PTR ThreadHandle;
    ULONG ProcessId;
    ULONG ThreadId;
    } 	APP_PROCESS_INFORMATION;

/* [async] */ void  RAiLaunchAdminProcess( 
    /* [in] */ PRPC_ASYNC_STATE RAiLaunchAdminProcess_AsyncHandle,
    handle_t hBinding,
    /* [string][unique][in] */ const wchar_t *ExecutablePath,
    /* [string][unique][in] */ const wchar_t *CommandLine,
    /* [in] */ ULONG StartFlags,
    /* [in] */ ULONG CreationFlags,
    /* [string][in] */ const wchar_t *CurrentDirectory,
    /* [string][in] */ const wchar_t *WindowStation,
    /* [in] */ APP_STARTUP_INFO *StartupInfo,
    /* [in] */ ULONG_PTR hWnd,
    /* [in] */ ULONG Timeout,
    /* [out] */ APP_PROCESS_INFORMATION *ProcessInformation,
    /* [out] */ ULONG *ElevationReason);



extern RPC_IF_HANDLE LaunchAdminProcess_v1_0_c_ifspec;
extern RPC_IF_HANDLE LaunchAdminProcess_v1_0_s_ifspec;
#endif /* __LaunchAdminProcess_INTERFACE_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif





/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/updater/server/win/updater_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.xx.xxxx 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_AMD64)


#pragma warning( disable: 4049 )  /* more than 64k source lines */
#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/
#pragma warning( disable: 4152 )  /* function/data pointer conversion in expression */

#define USE_STUBLESS_PROXY


/* verify that the <rpcproxy.h> version is high enough to compile this file*/
#ifndef __REDQ_RPCPROXY_H_VERSION__
#define __REQUIRED_RPCPROXY_H_VERSION__ 475
#endif


#include "rpcproxy.h"
#ifndef __RPCPROXY_H_VERSION__
#error this stub requires an updated version of <rpcproxy.h>
#endif /* __RPCPROXY_H_VERSION__ */


#include "updater_idl.h"

#define TYPE_FORMAT_STRING_SIZE   25                                
#define PROC_FORMAT_STRING_SIZE   215                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _updater_idl_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } updater_idl_MIDL_TYPE_FORMAT_STRING;

typedef struct _updater_idl_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } updater_idl_MIDL_PROC_FORMAT_STRING;

typedef struct _updater_idl_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } updater_idl_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};


extern const updater_idl_MIDL_TYPE_FORMAT_STRING updater_idl__MIDL_TypeFormatString;
extern const updater_idl_MIDL_PROC_FORMAT_STRING updater_idl__MIDL_ProcFormatString;
extern const updater_idl_MIDL_EXPR_FORMAT_STRING updater_idl__MIDL_ExprFormatString;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IUpdaterObserver_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdaterObserver_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IUpdater_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdater_ProxyInfo;



#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const updater_idl_MIDL_PROC_FORMAT_STRING updater_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure OnComplete */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x8 ),	/* 8 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter error_code */

/* 26 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 28 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 30 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 32 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 34 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 36 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */

/* 38 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 40 */	NdrFcLong( 0x0 ),	/* 0 */
/* 44 */	NdrFcShort( 0x3 ),	/* 3 */
/* 46 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 48 */	NdrFcShort( 0x0 ),	/* 0 */
/* 50 */	NdrFcShort( 0x8 ),	/* 8 */
/* 52 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 54 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */
/* 58 */	NdrFcShort( 0x0 ),	/* 0 */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 64 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 66 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 68 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 70 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 72 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Register */

/* 76 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 78 */	NdrFcLong( 0x0 ),	/* 0 */
/* 82 */	NdrFcShort( 0x4 ),	/* 4 */
/* 84 */	NdrFcShort( 0x38 ),	/* X64 Stack size/offset = 56 */
/* 86 */	NdrFcShort( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0x8 ),	/* 8 */
/* 90 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 92 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 94 */	NdrFcShort( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x0 ),	/* 0 */
/* 98 */	NdrFcShort( 0x0 ),	/* 0 */
/* 100 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 102 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 104 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 106 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter brand_code */

/* 108 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 110 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 112 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter tag */

/* 114 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 116 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 118 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter version */

/* 120 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 122 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 124 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter existence_checker_path */

/* 126 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 128 */	NdrFcShort( 0x28 ),	/* X64 Stack size/offset = 40 */
/* 130 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 132 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 134 */	NdrFcShort( 0x30 ),	/* X64 Stack size/offset = 48 */
/* 136 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 138 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 140 */	NdrFcLong( 0x0 ),	/* 0 */
/* 144 */	NdrFcShort( 0x5 ),	/* 5 */
/* 146 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 150 */	NdrFcShort( 0x8 ),	/* 8 */
/* 152 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 154 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 158 */	NdrFcShort( 0x0 ),	/* 0 */
/* 160 */	NdrFcShort( 0x0 ),	/* 0 */
/* 162 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter app_id */

/* 164 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 166 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 168 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 170 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 172 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 174 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 176 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 178 */	NdrFcLong( 0x0 ),	/* 0 */
/* 182 */	NdrFcShort( 0x6 ),	/* 6 */
/* 184 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 186 */	NdrFcShort( 0x0 ),	/* 0 */
/* 188 */	NdrFcShort( 0x8 ),	/* 8 */
/* 190 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 192 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 198 */	NdrFcShort( 0x0 ),	/* 0 */
/* 200 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter observer */

/* 202 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 204 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 206 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 208 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 210 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 212 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const updater_idl_MIDL_TYPE_FORMAT_STRING updater_idl__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/*  4 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/*  6 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/*  8 */	NdrFcLong( 0x7b416cfd ),	/* 2067885309 */
/* 12 */	NdrFcShort( 0x4216 ),	/* 16918 */
/* 14 */	NdrFcShort( 0x4fd6 ),	/* 20438 */
/* 16 */	0xbd,		/* 189 */
			0x83,		/* 131 */
/* 18 */	0x7c,		/* 124 */
			0x58,		/* 88 */
/* 20 */	0x60,		/* 96 */
			0x54,		/* 84 */
/* 22 */	0x67,		/* 103 */
			0x6e,		/* 110 */

			0x0
        }
    };


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IUpdaterObserver, ver. 0.0,
   GUID={0x7B416CFD,0x4216,0x4FD6,{0xBD,0x83,0x7C,0x58,0x60,0x54,0x67,0x6E}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdaterObserver_FormatStringOffsetTable[] =
    {
    0
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdaterObserver_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserver_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdaterObserver_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdaterObserver_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IUpdaterObserverProxyVtbl = 
{
    &IUpdaterObserver_ProxyInfo,
    &IID_IUpdaterObserver,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdaterObserver::OnComplete */
};

const CInterfaceStubVtbl _IUpdaterObserverStubVtbl =
{
    &IID_IUpdaterObserver,
    &IUpdaterObserver_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IUpdater, ver. 0.0,
   GUID={0x63B8FFB1,0x5314,0x48C9,{0x9C,0x57,0x93,0xEC,0x8B,0xC6,0x18,0x4B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdater_FormatStringOffsetTable[] =
    {
    38,
    76,
    138,
    176
    };

static const MIDL_STUBLESS_PROXY_INFO IUpdater_ProxyInfo =
    {
    &Object_StubDesc,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdater_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IUpdater_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    updater_idl__MIDL_ProcFormatString.Format,
    &IUpdater_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IUpdaterProxyVtbl = 
{
    &IUpdater_ProxyInfo,
    &IID_IUpdater,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IUpdater::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IUpdater::Register */ ,
    (void *) (INT_PTR) -1 /* IUpdater::Update */ ,
    (void *) (INT_PTR) -1 /* IUpdater::UpdateAll */
};

const CInterfaceStubVtbl _IUpdaterStubVtbl =
{
    &IID_IUpdater,
    &IUpdater_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};

static const MIDL_STUB_DESC Object_StubDesc = 
    {
    0,
    NdrOleAllocate,
    NdrOleFree,
    0,
    0,
    0,
    0,
    0,
    updater_idl__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x801026e, /* MIDL Version 8.1.622 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };

const CInterfaceProxyVtbl * const _updater_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IUpdaterProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IUpdaterObserverProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _updater_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IUpdaterStubVtbl,
    ( CInterfaceStubVtbl *) &_IUpdaterObserverStubVtbl,
    0
};

PCInterfaceName const _updater_idl_InterfaceNamesList[] = 
{
    "IUpdater",
    "IUpdaterObserver",
    0
};


#define _updater_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_idl, pIID, n)

int __stdcall _updater_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _updater_idl, 2, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _updater_idl, 2, *pIndex )
    
}

const ExtendedProxyFileInfo updater_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_idl_StubVtblList,
    (const PCInterfaceName * ) & _updater_idl_InterfaceNamesList,
    0, /* no delegation */
    & _updater_idl_IID_Lookup, 
    2,
    2,
    0, /* table of [async_uuid] interfaces */
    0, /* Filler1 */
    0, /* Filler2 */
    0  /* Filler3 */
};
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_AMD64)*/


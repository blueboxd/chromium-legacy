

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/updater/server/win/updater_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=ARM64 8.01.0622 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_ARM64)


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

#define TYPE_FORMAT_STRING_SIZE   7                                 
#define PROC_FORMAT_STRING_SIZE   189                               
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


extern const MIDL_SERVER_INFO IUpdater_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IUpdater_ProxyInfo;



#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const updater_idl_MIDL_PROC_FORMAT_STRING updater_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure CheckForUpdate */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x2 ),	/* 2 */
/* 26 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 30 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Register */

/* 42 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 44 */	NdrFcLong( 0x0 ),	/* 0 */
/* 48 */	NdrFcShort( 0x4 ),	/* 4 */
/* 50 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x8 ),	/* 8 */
/* 56 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 58 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	NdrFcShort( 0x6 ),	/* 6 */
/* 68 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 70 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 72 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 74 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 76 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 78 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 80 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter brand_code */

/* 82 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 84 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 86 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter tag */

/* 88 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 90 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 92 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter version */

/* 94 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 96 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 98 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Parameter existence_checker_path */

/* 100 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 102 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 104 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 106 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 108 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 110 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 112 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 114 */	NdrFcLong( 0x0 ),	/* 0 */
/* 118 */	NdrFcShort( 0x5 ),	/* 5 */
/* 120 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 122 */	NdrFcShort( 0x0 ),	/* 0 */
/* 124 */	NdrFcShort( 0x8 ),	/* 8 */
/* 126 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 128 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 130 */	NdrFcShort( 0x0 ),	/* 0 */
/* 132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x2 ),	/* 2 */
/* 138 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 140 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_id */

/* 142 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 144 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 146 */	NdrFcShort( 0x4 ),	/* Type Offset=4 */

	/* Return value */

/* 148 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 150 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 152 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateAll */

/* 154 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 156 */	NdrFcLong( 0x0 ),	/* 0 */
/* 160 */	NdrFcShort( 0x6 ),	/* 6 */
/* 162 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 164 */	NdrFcShort( 0x0 ),	/* 0 */
/* 166 */	NdrFcShort( 0x8 ),	/* 8 */
/* 168 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 170 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 172 */	NdrFcShort( 0x0 ),	/* 0 */
/* 174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 176 */	NdrFcShort( 0x0 ),	/* 0 */
/* 178 */	NdrFcShort( 0x1 ),	/* 1 */
/* 180 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 182 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 184 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 186 */	0x8,		/* FC_LONG */
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

			0x0
        }
    };


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IUpdater, ver. 0.0,
   GUID={0x63B8FFB1,0x5314,0x48C9,{0x9C,0x57,0x93,0xEC,0x8B,0xC6,0x18,0x4B}} */

#pragma code_seg(".orpc")
static const unsigned short IUpdater_FormatStringOffsetTable[] =
    {
    0,
    42,
    112,
    154
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
    0
};

const CInterfaceStubVtbl * const _updater_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IUpdaterStubVtbl,
    0
};

PCInterfaceName const _updater_idl_InterfaceNamesList[] = 
{
    "IUpdater",
    0
};


#define _updater_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _updater_idl, pIID, n)

int __stdcall _updater_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    
    if(!_updater_idl_CHECK_IID(0))
        {
        *pIndex = 0;
        return 1;
        }

    return 0;
}

const ExtendedProxyFileInfo updater_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _updater_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _updater_idl_StubVtblList,
    (const PCInterfaceName * ) & _updater_idl_InterfaceNamesList,
    0, /* no delegation */
    & _updater_idl_IID_Lookup, 
    1,
    2,
    0, /* table of [async_uuid] interfaces */
    0, /* Filler1 */
    0, /* Filler2 */
    0  /* Filler3 */
};
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_ARM64)*/


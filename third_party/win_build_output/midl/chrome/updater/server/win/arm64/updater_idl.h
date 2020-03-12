

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


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

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __updater_idl_h__
#define __updater_idl_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IUpdater_FWD_DEFINED__
#define __IUpdater_FWD_DEFINED__
typedef interface IUpdater IUpdater;

#endif 	/* __IUpdater_FWD_DEFINED__ */


#ifndef __IUpdater_FWD_DEFINED__
#define __IUpdater_FWD_DEFINED__
typedef interface IUpdater IUpdater;

#endif 	/* __IUpdater_FWD_DEFINED__ */


#ifndef __UpdaterClass_FWD_DEFINED__
#define __UpdaterClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class UpdaterClass UpdaterClass;
#else
typedef struct UpdaterClass UpdaterClass;
#endif /* __cplusplus */

#endif 	/* __UpdaterClass_FWD_DEFINED__ */


#ifndef __UpdaterServiceClass_FWD_DEFINED__
#define __UpdaterServiceClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class UpdaterServiceClass UpdaterServiceClass;
#else
typedef struct UpdaterServiceClass UpdaterServiceClass;
#endif /* __cplusplus */

#endif 	/* __UpdaterServiceClass_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IUpdater_INTERFACE_DEFINED__
#define __IUpdater_INTERFACE_DEFINED__

/* interface IUpdater */
/* [unique][helpstring][uuid][dual][object] */ 


EXTERN_C const IID IID_IUpdater;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("63B8FFB1-5314-48C9-9C57-93EC8BC6184B")
    IUpdater : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CheckForUpdate( 
            /* [string][in] */ const WCHAR *app_id) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Register( 
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *brand_code,
            /* [string][in] */ const WCHAR *tag,
            /* [string][in] */ const WCHAR *version,
            /* [string][in] */ const WCHAR *existence_checker_path) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Update( 
            /* [string][in] */ const WCHAR *app_id) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UpdateAll( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IUpdaterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IUpdater * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IUpdater * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IUpdater * This);
        
        HRESULT ( STDMETHODCALLTYPE *CheckForUpdate )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id);
        
        HRESULT ( STDMETHODCALLTYPE *Register )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id,
            /* [string][in] */ const WCHAR *brand_code,
            /* [string][in] */ const WCHAR *tag,
            /* [string][in] */ const WCHAR *version,
            /* [string][in] */ const WCHAR *existence_checker_path);
        
        HRESULT ( STDMETHODCALLTYPE *Update )( 
            IUpdater * This,
            /* [string][in] */ const WCHAR *app_id);
        
        HRESULT ( STDMETHODCALLTYPE *UpdateAll )( 
            IUpdater * This);
        
        END_INTERFACE
    } IUpdaterVtbl;

    interface IUpdater
    {
        CONST_VTBL struct IUpdaterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IUpdater_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IUpdater_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IUpdater_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IUpdater_CheckForUpdate(This,app_id)	\
    ( (This)->lpVtbl -> CheckForUpdate(This,app_id) ) 

#define IUpdater_Register(This,app_id,brand_code,tag,version,existence_checker_path)	\
    ( (This)->lpVtbl -> Register(This,app_id,brand_code,tag,version,existence_checker_path) ) 

#define IUpdater_Update(This,app_id)	\
    ( (This)->lpVtbl -> Update(This,app_id) ) 

#define IUpdater_UpdateAll(This)	\
    ( (This)->lpVtbl -> UpdateAll(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IUpdater_INTERFACE_DEFINED__ */



#ifndef __UpdaterLib_LIBRARY_DEFINED__
#define __UpdaterLib_LIBRARY_DEFINED__

/* library UpdaterLib */
/* [helpstring][version][uuid] */ 



EXTERN_C const IID LIBID_UpdaterLib;

EXTERN_C const CLSID CLSID_UpdaterClass;

#ifdef __cplusplus

class DECLSPEC_UUID("158428A4-6014-4978-83BA-9FAD0DABE791")
UpdaterClass;
#endif

EXTERN_C const CLSID CLSID_UpdaterServiceClass;

#ifdef __cplusplus

class DECLSPEC_UUID("415FD747-D79E-42D7-93AC-1BA6E5FD4E93")
UpdaterServiceClass;
#endif
#endif /* __UpdaterLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif



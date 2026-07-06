

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 6.00.0366 */
/* at Mon Jul 06 14:01:56 2026
 */
/* Compiler settings for .\CustPalette.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_IInspData,0x621BA68B,0x5688,0x4BD8,0x80,0xE6,0xC0,0x09,0x7F,0xAD,0x9A,0xC9);


MIDL_DEFINE_GUID(IID, IID_IDWGPreview,0xE7FF7C38,0x71E7,0x4EAD,0x80,0xC7,0xFA,0x26,0x3D,0x8C,0xEA,0x8E);


MIDL_DEFINE_GUID(IID, LIBID_AsdkCustPaletteLib,0x8A76C9A0,0x80F0,0x4F7B,0x92,0x47,0x7C,0xC1,0x73,0x09,0x62,0xAC);


MIDL_DEFINE_GUID(CLSID, CLSID_InspData,0x27116BE4,0x11DA,0x4D43,0x95,0x2F,0xEF,0x03,0xEA,0xA6,0x28,0x84);


MIDL_DEFINE_GUID(CLSID, CLSID_DWGPreview,0x44EF9F52,0x159F,0x480F,0x9A,0xD8,0x62,0x89,0xC8,0xC4,0x1A,0xDE);


MIDL_DEFINE_GUID(CLSID, CLSID_PropPage,0x8D92B6D5,0xEFBA,0x4AA1,0x93,0xF4,0xF8,0xC3,0xD3,0x4F,0xC1,0xFB);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif




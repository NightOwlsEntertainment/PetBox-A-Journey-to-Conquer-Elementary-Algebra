////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2013.
// -------------------------------------------------------------------------
//  File name:   CryPlatform.h
//  Version:     v1.00
//  Created:     31/01/2013 by Christopher Bolte.
//  Compilers:   Visual Studio.NET
//  Description: Interface for the platform specific function libraries
//               Include this file instead of windows.h and similar platform specific header files
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _CRY_PLATFORM_H_
#define _CRY_PLATFORM_H_

////////////////////////////////////////////////////////////////////////////
// this define allows including the detail headers which are setting platform specific settings
#define CRYPLATFROM_ALLOW_DETAIL_INCLUDES


////////////////////////////////////////////////////////////////////////////
// some ifdef selection to include the correct platform implementation
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
#	include "CryPlatform.Win32.h"
#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
#	include "CryPlatform.Win64.h"
#elif CRY_PLATFORM_DURANGO
#	include "CryPlatform.Durango.h"
#elif CRY_PLATFORM_ORBIS
#	include "CryPlatform.Orbis.h"
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
#	include "CryPlatform.Linux.h"
#elif CRY_PLATFORM_APPLE
#	include "CryPlatform.Darwin.h"
#else
#	error Current Platform not supported by CryPlatform. Please provide a concrete implementation library.
#endif

// include forward declarations of all *::detail functions which live in the specific CryPlatfrom libraries
#include "CryPlatform.fwd.h"


///////////////////////////////////////////////////////////////////////////
// verify that all requiered macros are set
///////////////////////////////////////////////////////////////////////////	
#if !defined(CRYPLATFORM_INTERLOCKEDSLIST_HEADER_SIZE)
#	error CRYPLATFORM_INTERLOCKEDSLIST_HEADER_SIZE not defined for current platform
#endif

#if !defined(CRYPLATFORM_INTERLOCKEDSLIST_HEADER_ALIGNMENT)
#	error CRYPLATFORM_INTERLOCKEDSLIST_HEADER_ALIGNMENT not defined for current platform
#endif

#if !defined(CRYPLATFORM_INTERLOCKEDSLIST_ELEMENT_SIZE)
#	error CRYPLATFORM_INTERLOCKEDSLIST_ELEMENT_SIZE not defined for current platform
#endif

#if !defined(CRYPLATFORM_INTERLOCKEDSLIST_ELEMENT_ALIGNMENT)
#	error CRYPLATFORM_INTERLOCKEDSLIST_ELEMENT_ALIGNMENT not defined for current platform
#endif

#if !defined(__DETAIL__LINK_THIRD_PARTY_LIBRARY)
#	error __DETAIL__LINK_THIRD_PARTY_LIBRARY not defined for current platform
#endif

#if !defined(__DETAIL__LINK_SYSTEM_PARTY_LIBRARY)
#	error __DETAIL__LINK_SYSTEM_PARTY_LIBRARY not defined for current platform
#endif

////////////////////////////////////////////////////////////////////////////
// Multithreading primitive functions
////////////////////////////////////////////////////////////////////////////
namespace CryMT {

	////////////////////////////////////////////////////////////////////////////
	// Interlocked Single Linked List functions - without ABA Problem
	////////////////////////////////////////////////////////////////////////////
	//
	// SInterlockedSListHeader:		Header of a interlocked singled linked list. Must be initialized before using.
	// SInterlockedSListElement:	Element of a interlocked single linked list.
	//
	DEFINE_OPAQUE_TYPE( SInterlockedSListHeader, CRYPLATFORM_INTERLOCKEDSLIST_HEADER_SIZE, CRYPLATFORM_INTERLOCKEDSLIST_HEADER_ALIGNMENT );
	DEFINE_OPAQUE_TYPE( SInterlockedSListElement, CRYPLATFORM_INTERLOCKEDSLIST_ELEMENT_SIZE, CRYPLATFORM_INTERLOCKEDSLIST_ELEMENT_ALIGNMENT );

	// Initialize a interlocked single linked list header. Must be called once before using the list.
	void InterlockedSListInitialize( SInterlockedSListHeader *pHeader );

	// Flush a interlocked single linked list. Afterwards the header points to an empty list. Returns XXX
	void InterlockedSListFlush( SInterlockedSListHeader *pHeader );
	
	// Pushes a new element at the beginning of the interlocked single linked list.
	void InterlockedSListPush( SInterlockedSListHeader *pHeader, SInterlockedSListElement *pElement );
	
	// Pops the first element from the interlocked single link list. Returns NULL if the list was empty.
	SInterlockedSListElement* InterlockedSListPop( SInterlockedSListHeader *pHeader );
	  	
} // namespace CryMT


////////////////////////////////////////////////////////////////////////////
// Interlocked Single Linked List functions Implementations
////////////////////////////////////////////////////////////////////////////	
inline void CryMT::InterlockedSListInitialize( SInterlockedSListHeader *pHeader )
{
	CryMT::detail::InterlockedSListInitialize(pHeader);
}

////////////////////////////////////////////////////////////////////////////
inline void CryMT::InterlockedSListFlush( CryMT::SInterlockedSListHeader *pHeader )
{
	CryMT::detail::InterlockedSListFlush(pHeader);
}
	
////////////////////////////////////////////////////////////////////////////
inline void CryMT::InterlockedSListPush( CryMT::SInterlockedSListHeader *pHeader, CryMT::SInterlockedSListElement *pElement )
{
	CryMT::detail::InterlockedSListPush(pHeader, pElement);
}
	
////////////////////////////////////////////////////////////////////////////
inline CryMT::SInterlockedSListElement* CryMT::InterlockedSListPop( CryMT::SInterlockedSListHeader *pHeader )
{
	return CryMT::detail::InterlockedSListPop(pHeader);
}

#ifdef CRY_NO_PRAGMA_LIB
#define LINK_THIRD_PARTY_LIBRARY( name )
#define LINK_SYSTEM_LIBRARY( name )
#else
////////////////////////////////////////////////////////////////////////////
// Include a third party library. The path has to be specified
// relative to the Code/ folder. In addition the path has to be specified
// as a literal, not as a string, and forward slashes have to be used eg:
// LINK_THIRD_PARTY_LIBRARY(SDK/MyLib/lib/MyLib.a)
#define LINK_THIRD_PARTY_LIBRARY( name ) __DETAIL__LINK_THIRD_PARTY_LIBRARY(name)

////////////////////////////////////////////////////////////////////////////
// Include a platform library.
#define LINK_SYSTEM_LIBRARY( name ) __DETAIL__LINK_SYSTEM_PARTY_LIBRARY(name)
#endif //CRY_NO_PRAGMA_LIB

////////////////////////////////////////////////////////////////////////////
// disallow including of detail header
#undef CRYPLATFROM_ALLOW_DETAIL_INCLUDES

#endif // _CRY_PLATFORM_H_
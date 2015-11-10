/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2010.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Shared parameters manager interface.

-------------------------------------------------------------------------
History:
- 15:07:2010: Created by Paul Slinger

*************************************************************************/

#ifndef __ISHAREDPARAMSMANAGER_H__
#define __ISHAREDPARAMSMANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declaration of shared parameters type information class.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSharedParamsTypeInfo;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declaration of shared parameters interface class.
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISharedParams;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared parameters pointers.
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SHARED_POINTERS(ISharedParams);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared parameters manager interface class.
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISharedParamsManager
{
	public:
		virtual	~ISharedParamsManager(){}
		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Generate unique 32 bit crc.
		//
		// Params: pName - unique name
		//
		// Return: unique 32 bit crc
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual uint32 GenerateCRC32(const char *pName) = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Register shared parameters.
		//
		// Params: crc32        - unique 32 bit crc
		//				 sharedParams - shared parameters
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual ISharedParamsConstPtr Register(uint32 crc32, const ISharedParams &sharedParams) = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Register shared parameters.
		//
		// Params: pName        - unique name
		//				 sharedParams - shared parameters
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual ISharedParamsConstPtr Register(const char *pName, const ISharedParams &sharedParams) = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Remove shared parameters.
		//
		// Params: crc32 - unique 32 bit crc
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual void Remove(uint32 crc32) = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Remove shared parameters.
		//
		// Params: pName - unique name
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual void Remove(const char *pName) = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Remove all shared parameters by type.
		//
		// Params: typeInfo - type information of to be removed
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual void RemoveByType(const CSharedParamsTypeInfo &typeInfo) = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Get shared parameters.
		//
		// Params: crc32 - unique 32 bit crc
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual ISharedParamsConstPtr Get(uint32 crc32) const = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Get shared parameters.
		//
		// Params: pName - unique name
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual ISharedParamsConstPtr Get(const char *pName) const = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Reset.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual void Reset() = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Release.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual void Release() = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Get memory statistics.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		virtual void GetMemoryStatistics(ICrySizer *pSizer) = 0;
};

#endif //__ISHAREDPARAMSMANAGER_H__
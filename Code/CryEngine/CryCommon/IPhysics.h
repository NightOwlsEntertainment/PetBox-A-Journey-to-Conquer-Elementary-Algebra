#ifndef cryphysics_h
#define cryphysics_h

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//
#ifdef PHYSICS_EXPORTS
	#define CRYPHYSICS_API DLL_EXPORT
#else
	#define CRYPHYSICS_API DLL_IMPORT
#endif

#define vector_class Vec3_tpl


#include <CrySizer.h>

//////////////////////////////////////////////////////////////////////////
// IDs that can be used for foreign id.
//////////////////////////////////////////////////////////////////////////
enum EPhysicsForeignIds
{
	PHYS_FOREIGN_ID_TERRAIN = 0,
	PHYS_FOREIGN_ID_STATIC = 1,
	PHYS_FOREIGN_ID_ENTITY = 2,
	PHYS_FOREIGN_ID_FOLIAGE = 3,
	PHYS_FOREIGN_ID_ROPE = 4,
	PHYS_FOREIGN_ID_SOUND_OBSTRUCTION = 5,
	PHYS_FOREIGN_ID_SOUND_PROXY_OBSTRUCTION = 6,
	PHYS_FOREIGN_ID_SOUND_REVERB_OBSTRUCTION = 7,
	PHYS_FOREIGN_ID_WATERVOLUME = 8,
	PHYS_FOREIGN_ID_BREAKABLE_GLASS = 9,
	PHYS_FOREIGN_ID_BREAKABLE_GLASS_FRAGMENT = 10,
	PHYS_FOREIGN_ID_RIGID_PARTICLE = 11,
	PHYS_FOREIGN_ID_RESERVED1 = 12,
	PHYS_FOREIGN_ID_RAGDOLL = 13,

	PHYS_FOREIGN_ID_USER = 100, // All user defined foreign ids should start from this enum.
};


//#include "utils.h"
#include "Cry_Math.h"
#include "primitives.h"
#include <physinterface.h> // <> required for Interfuscator

extern "C" CRYPHYSICS_API IPhysicalWorld *CreatePhysicalWorld(struct ISystem *pLog);

#endif

// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#include "stdafx.h"
#include "AudioSystemImpl_sdlmixer.h"
#include "AudioSystemImplCVars_sdlmixer.h"
#include <IAudioSystem.h>
#include <platform_impl.h>
#include <IEngineModule.h>
#include <CryExtension/Impl/ClassWeaver.h>

CSoundAllocator				g_SDLMixerImplMemoryPool;
CAudioLogger					g_SDLMixerImplLogger;
CAudioSystemImplCVars g_SDLMixerImplCVars;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioImplSDL : public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule)
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryAudioImplSDL, "CryAudioImplSDLMixer", 0x8030c0d1905b4031, 0xa3785a8b53125f3f)

	//////////////////////////////////////////////////////////////////////////
	virtual const char *GetName() {return "CryAudioImplSDLMixer";}
	virtual const char *GetCategory() {return "CryAudio";}

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment &env,const SSystemInitParams &initParams)
	{
		bool bSuccess = false;

		// initialize memory pools

		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "SDL Mixer Audio Implementation Memory Pool Primary");
		size_t const nPoolSize		= g_SDLMixerImplCVars.m_nPrimaryPoolSize << 10;
		uint8* const pPoolMemory	= new uint8[nPoolSize];
		g_SDLMixerImplMemoryPool.InitMem(nPoolSize, pPoolMemory, "SDL Mixer Implementation Audio Pool");

		POOL_NEW_CREATE(CAudioSystemImpl_sdlmixer, pImpl);

		if (pImpl	!= nullptr)
		{
			g_SDLMixerImplLogger.Log(eALT_ALWAYS, "CryAudioImplSDLMixer loaded");

			SAudioRequest oAudioRequestData;
			oAudioRequestData.nFlags = eARF_PRIORITY_HIGH | eARF_EXECUTE_BLOCKING;

			SAudioManagerRequestData<eAMRT_SET_AUDIO_IMPL> oAMData(pImpl);
			oAudioRequestData.pData = &oAMData;
			env.pAudioSystem->PushRequest(oAudioRequestData);

			bSuccess = true;
		}
		else
		{
			g_SDLMixerImplLogger.Log(eALT_ALWAYS, "CryAudioImplSDLMixer failed to load");
		}

		return bSuccess;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioImplSDL)

CEngineModule_CryAudioImplSDL::CEngineModule_CryAudioImplSDL()
{
	g_SDLMixerImplCVars.RegisterVariables();
}

CEngineModule_CryAudioImplSDL::~CEngineModule_CryAudioImplSDL()
{
}

#include <CrtDebugStats.h>

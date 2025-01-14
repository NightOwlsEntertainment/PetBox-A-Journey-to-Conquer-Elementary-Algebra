#include "stdafx.h"
#include "SDLMixerSoundEngine.h"
#include "SDLMixerSoundEngineUtil.h"
#include <CryFile.h>
#include <CryPath.h>

#include <SDL.h>
#include <SDL_rwops.h>
#include <SDL_mixer.h>

#define SDL_MIXER_NUM_CHANNELS 512
#define SDL_MIXER_PROJECT_PATH "/sounds/sdlmixer/"

namespace SDLMixer
{
	static const int g_nSupportedFormats = MIX_INIT_OGG;
	static const int g_nNumMixChannels = SDL_MIXER_NUM_CHANNELS;

	static const TEventID SDL_MIXER_INVALID_EVENT_ID = 0;
	static const TSampleID SDL_MIXER_INVALID_SAMPLE_ID = 0;
	const int g_nSampleRate = 48000;
	const int g_nBufferSize = 4096;

	namespace SoundEngine
	{
		// Samples
		string g_sampleDataRootDir;
		typedef std::map<TSampleID, Mix_Chunk*> TSampleDataMap;
		TSampleDataMap g_sampleData;

		typedef std::map<TSampleID, string> TSampleNameMap;
		TSampleNameMap g_sampleNames;

		// Channels
		struct SChannelData
		{
			SSDLMixerAudioObjectData* pAudioObject;
		};
		SChannelData g_channels[SDL_MIXER_NUM_CHANNELS];

		typedef std::queue<int> TChannelQueue;
		TChannelQueue g_freeChannels;

		enum EChannelFinishedRequestQueueID
		{
			eCFRQID_ONE	= 0,
			eCFRQID_TWO	= 1,
			eCFRQID_COUNT
		};
		typedef std::deque<int> TChannelFinishedRequests;
		TChannelFinishedRequests g_channelFinishedRequests[eCFRQID_COUNT];
		CryCriticalSection g_channelFinishedCriticalSection;

		// Audio Objects
		typedef std::vector<SSDLMixerAudioObjectData*> TAudioObjectList;
		TAudioObjectList g_audioObjects;

		// Listeners
		CAudioObjectTransformation g_listenerPosition;
		bool g_bListenerPosChanged;

		bool g_bMuted;

		TFnEventCallback g_fnEventFinishedCallback;

		void RegisterEventFinishedCallback(TFnEventCallback pCallbackFunction)
		{
			g_fnEventFinishedCallback = pCallbackFunction;
		}

		void EventFinishedPlaying(TAudioEventID nEventID)
		{
			if (g_fnEventFinishedCallback)
			{
				g_fnEventFinishedCallback(nEventID);
			}
		}

		void ProcessChannelFinishedRequests(TChannelFinishedRequests& queue)
		{
			if (!queue.empty())
			{
				TChannelFinishedRequests::const_iterator requestsIt = queue.begin();
				const TChannelFinishedRequests::const_iterator requestsEnd = queue.end();
				for (; requestsIt != requestsEnd; ++requestsIt)
				{
					const int nChannel = *requestsIt;
					SSDLMixerAudioObjectData* pAudioObject = g_channels[nChannel].pAudioObject;
					if (pAudioObject)
					{
						TEventInstanceSet::iterator eventsIt = pAudioObject->events.begin();
						const TEventInstanceSet::iterator eventsEnd = pAudioObject->events.end();
						for (; eventsIt != eventsEnd; ++eventsIt)
						{
							SSDLMixerEventInstanceData* pEventInstance = *eventsIt;
							if (pEventInstance)
							{
								TChannelSet::const_iterator channelIt = std::find(pEventInstance->channels.begin(), pEventInstance->channels.end(), nChannel);
								if (channelIt != pEventInstance->channels.end())
								{
									pEventInstance->channels.erase(channelIt);
									if (pEventInstance->channels.empty())
									{
										pAudioObject->events.erase(eventsIt);
										EventFinishedPlaying(pEventInstance->nEventID);
									}
									break;
								}
							}
						}
						g_channels[nChannel].pAudioObject = nullptr;
					}
					g_freeChannels.push(nChannel);
				}
				queue.clear();
			}
		}

		void ChannelFinishedPlaying(int nChannel)
		{
			if (nChannel >= 0 && nChannel < g_nNumMixChannels)
			{
				CryAutoLock<CryCriticalSection> autoLock(g_channelFinishedCriticalSection);
				g_channelFinishedRequests[eCFRQID_ONE].push_back(nChannel);
			}
		}

		void LoadMetadata(const string& sSDLMixerAssetPath)
		{
			g_sampleDataRootDir = PathUtil::GetPath(sSDLMixerAssetPath)  + "/";
			_finddata_t fd;
			ICryPak* pCryPak = gEnv->pCryPak;
			intptr_t handle = pCryPak->FindFirst(sSDLMixerAssetPath + "*.*", &fd);
			if (handle != -1)
			{
				do
				{
					const string sName = fd.name;
					if (sName != "." && sName != ".." && !sName.empty())
					{

						if (sName.find(".wav") != string::npos || sName.find(".ogg") != string::npos)
						{
							// For now there's a 1 to 1 mapping between sample files and events
							g_sampleNames[GetIDFromFilePath(sName)] = sName;
						}
					}
				}
				while (pCryPak->FindNext(handle, &fd) >= 0);
				pCryPak->FindClose(handle);
			}

		}

		bool Init()
		{
			if (SDL_Init(SDL_INIT_AUDIO) < 0)
			{
				g_SDLMixerImplLogger.Log(eALT_ERROR, "SDL::SDL_Init() returned: %s", SDL_GetError());
				return false;
			}

			int loadedFormats = Mix_Init(g_nSupportedFormats);
			if ((loadedFormats & g_nSupportedFormats) != g_nSupportedFormats)
			{
				g_SDLMixerImplLogger.Log(eALT_ERROR, "SDLMixer::Mix_Init() failed to init support for format flags %d with error \"%s\"", g_nSupportedFormats, Mix_GetError());
				return false;
			}

			if (Mix_OpenAudio(g_nSampleRate, MIX_DEFAULT_FORMAT, 2, g_nBufferSize) < 0)
			{
				g_SDLMixerImplLogger.Log(eALT_ERROR, "SDLMixer::Mix_OpenAudio() failed to init the SDL Mixer API with error \"%s\"", Mix_GetError());
				return false;
			}

			Mix_AllocateChannels(g_nNumMixChannels);

			g_bMuted = false;
			Mix_Volume(-1, SDL_MIX_MAXVOLUME);
			for (int i = 0; i < SDL_MIXER_NUM_CHANNELS; ++i)
			{
				g_freeChannels.push(i);
			}

			Mix_ChannelFinished(ChannelFinishedPlaying);

			LoadMetadata(PathUtil::GetGameFolder() + SDL_MIXER_PROJECT_PATH);
			g_bListenerPosChanged = false;

			return true;
		}

		void FreeAllSampleData()
		{
			Mix_HaltChannel(-1);
			TSampleDataMap::const_iterator it = g_sampleData.begin();
			TSampleDataMap::const_iterator end = g_sampleData.end();
			for (; it != end; ++it)
			{
				Mix_FreeChunk(it->second);
			}
			g_sampleData.clear();
		}

		void Release()
		{
			FreeAllSampleData();
			g_audioObjects.clear();
			Mix_Quit();
			Mix_CloseAudio();
			SDL_Quit();
		}

		void Refresh()
		{
			FreeAllSampleData();
			LoadMetadata(PathUtil::GetGameFolder() + SDL_MIXER_PROJECT_PATH);
		}

		bool LoadSampleImpl(const TSampleID nID, const string& sSamplePath)
		{
			bool bSuccess = true;
			Mix_Chunk* pSample = Mix_LoadWAV(sSamplePath.c_str());
			if (pSample != nullptr)
			{
				g_sampleData[nID] = pSample;
				g_sampleNames[nID] = sSamplePath;
			}
			else
			{
				// Sample could be inside a pak file so we need to open it manually and load it from the raw file
				const size_t nFileSize = gEnv->pCryPak->FGetSize(sSamplePath);
				FILE* const pFile = gEnv->pCryPak->FOpen(sSamplePath, "rbx", ICryPak::FOPEN_HINT_DIRECT_OPERATION);
				if (pFile && nFileSize > 0)
				{
					void* const pData = AUDIO_ALLOCATOR_MEMORY_POOL.AllocateRaw(nFileSize, AUDIO_MEMORY_ALIGNMENT, "SDLMixerSample");
					gEnv->pCryPak->FReadRawAll(pData, nFileSize, pFile);
					const TSampleID nNewID = LoadSample(pData, nFileSize, sSamplePath);
					if (nNewID == SDL_MIXER_INVALID_SAMPLE_ID)
					{
						g_SDLMixerImplLogger.Log(eALT_ERROR, "SDL Mixer failed to load sample %s. Error: \"%s\"", sSamplePath.c_str(), Mix_GetError());
						bSuccess = false;
					}
					AUDIO_ALLOCATOR_MEMORY_POOL.Free(pData);
				}
			}
			return bSuccess;
		}

		const TSampleID LoadSample(const string& sSampleFilePath)
		{
			const TSampleID nID = GetIDFromFilePath(PathUtil::GetFile(sSampleFilePath));
			if (stl::find_in_map(g_sampleData, nID, nullptr) == nullptr)
			{
				if (!LoadSampleImpl(nID, sSampleFilePath))
				{
					return SDL_MIXER_INVALID_SAMPLE_ID;
				}
			}
			return nID;
		}

		const TSampleID LoadSample(void* pMemory, const size_t nSize, const string& sSamplePath)
		{
			const TSampleID nID = GetIDFromFilePath(sSamplePath);
			Mix_Chunk* pSample = stl::find_in_map(g_sampleData, nID, nullptr);
			if (pSample != nullptr)
			{
				Mix_FreeChunk(pSample);
				g_SDLMixerImplLogger.Log(eALT_WARNING, "Loading sample %s which had already been loaded", sSamplePath.c_str());
			}
			SDL_RWops* pData = SDL_RWFromMem(pMemory, nSize);
			if (pData)
			{
				Mix_Chunk* pSample = Mix_LoadWAV_RW(pData, 0);
				if (pSample != nullptr)
				{
					g_sampleData[nID] = pSample;
					g_sampleNames[nID] = sSamplePath;
					return nID;
				}
				else
				{
					g_SDLMixerImplLogger.Log(eALT_ERROR, "SDL Mixer failed to load sample. Error: \"%s\"", Mix_GetError());
				}
			}
			else
			{
				g_SDLMixerImplLogger.Log(eALT_ERROR, "SDL Mixer failed to transform the audio data. Error: \"%s\"", SDL_GetError());
			}
			return SDL_MIXER_INVALID_SAMPLE_ID;
		}

		void UnloadSample(const TSampleID nID)
		{
			Mix_Chunk* pSample = stl::find_in_map(g_sampleData, nID, nullptr);
			if (pSample != nullptr)
			{
				Mix_FreeChunk(pSample);
				stl::member_find_and_erase(g_sampleData, nID);
			}
			else
			{
				g_SDLMixerImplLogger.Log(eALT_ERROR, "Could not find sample with id %d", nID);
			}
		}

		void Pause()
		{
			Mix_Pause(-1);
			Mix_PauseMusic();
		}

		void Resume()
		{
			Mix_Resume(-1);
			Mix_ResumeMusic();
		}

		void Stop()
		{
			Mix_HaltChannel(-1);
		}

		void Mute()
		{
			Mix_Volume(-1, 0);
			g_bMuted = true;
		}

		void UnMute()
		{
			TAudioObjectList::const_iterator audioObjectIt = g_audioObjects.begin();
			const TAudioObjectList::const_iterator audioObjectEnd = g_audioObjects.end();
			for (; audioObjectIt != audioObjectEnd; ++audioObjectIt)
			{
				SSDLMixerAudioObjectData* pAudioObject = *audioObjectIt;
				if (pAudioObject)
				{
					TEventInstanceSet::const_iterator eventIt = pAudioObject->events.begin();
					const TEventInstanceSet::const_iterator eventEnd = pAudioObject->events.end();
					for (; eventIt != eventEnd; ++eventIt)
					{
						SSDLMixerEventInstanceData* pEventInstance = *eventIt;
						if (pEventInstance)
						{
							TChannelSet::const_iterator channelIt = pEventInstance->channels.begin();
							TChannelSet::const_iterator channelEnd = pEventInstance->channels.end();
							for (; channelIt != channelEnd; ++channelIt)
							{
								Mix_Volume(*channelIt, pEventInstance->pStaticData->nVolume);
							}
						}
					}
				}
			}
			g_bMuted = false;
		}

		void SetChannelPosition(SSDLMixerEventInstanceData* const pEventInstance, const int channelID, const float fDistance, const float fAngle)
		{
			static const uint8 nSDLMaxDistance = 255;
			const float fMin = pEventInstance->pStaticData->fAttenuationMinDistance;
			const float fMax = pEventInstance->pStaticData->fAttenuationMaxDistance;
			if (fMin <= fMax)
			{
				uint8 nDistance = 0;
				if (fMax >= 0.0f && fDistance > fMin)
				{
					if (fMin != fMax)
					{
						const float fFinalDistance = fDistance - fMin;
						const float fRange = fMax - fMin;
						nDistance = static_cast<uint8>((std::min((fFinalDistance / fRange), 1.0f) * nSDLMaxDistance) + 0.5f);
					}
					else
					{
						nDistance = nSDLMaxDistance;
					}
				}
				//Temp code, to be reviewed during the SetChannelPosition rewrite:
				Mix_SetDistance(channelID, nDistance);

				if (pEventInstance->pStaticData->bPanningEnabled)
				{
					//Temp code, to be reviewed during the SetChannelPosition rewrite:
					float const fAbsAngle = fabs(fAngle);
					float const fFrontAngle = (fAngle > 0.0f ? 1.0f : -1.0f) * (fAbsAngle > 90.0f ? 180.f - fAbsAngle : fAbsAngle);
					float const fRightVolume = (fFrontAngle + 90.0f) / 180.0f;
					float const fLeftVolume = 1.0f - fRightVolume;
					Mix_SetPanning(channelID,
					               static_cast<uint8>(255.0f * fLeftVolume),
					               static_cast<uint8>(255.0f * fRightVolume));
				}
			}
			else
			{
				g_SDLMixerImplLogger.Log(eALT_ERROR, "The minimum attenuation distance value is higher than the maximum");
			}
		}

		bool ExecuteEvent(SSDLMixerAudioObjectData* const pAudioObject, SSDLMixerEventStaticData const* const pEventStaticData, SSDLMixerEventInstanceData* const pEventInstance)
		{
			bool bSuccess = false;

			if (pAudioObject && pEventStaticData && pEventInstance)
			{
				if (pEventStaticData->bStartEvent) // start playing samples
				{
					pEventInstance->pStaticData = pEventStaticData;

					Mix_Chunk* pSample = stl::find_in_map(g_sampleData, pEventStaticData->nSampleID, nullptr);
					if (pSample == nullptr)
					{
						// Trying to play sample that hasn't been loaded yet, load it in place
						// NOTE: This should be avoided as it can cause lag in audio playback
						const string sSampleName = g_sampleNames[pEventStaticData->nSampleID];
						if (LoadSampleImpl(GetIDFromFilePath(sSampleName), g_sampleDataRootDir + sSampleName))
						{
							pSample = stl::find_in_map(g_sampleData, pEventStaticData->nSampleID, nullptr);
						}
						if (pSample == nullptr)
						{
							return false;
						}
					}

					int nLoopCount = pEventStaticData->nLoopCount;
					if (nLoopCount > 0)
					{
						// For SDL Mixer 0 loops means play only once, 1 loop play twice, etc ...
						--nLoopCount;
					}

					if (!g_freeChannels.empty())
					{
						int nChannelID = Mix_PlayChannel(g_freeChannels.front(), pSample, nLoopCount);
						if (nChannelID >= 0)
						{
							g_freeChannels.pop();
							Mix_Volume(nChannelID, g_bMuted ? 0 : pEventStaticData->nVolume);

							// Get distance and angle from the listener to the audio object
							float fDistance = 0.0f;
							float fAngle = 0.0f;
							GetDistanceAngleToObject(g_listenerPosition, pAudioObject->position, fDistance, fAngle);
							SetChannelPosition(pEventInstance, nChannelID, fDistance, fAngle);

							g_channels[nChannelID].pAudioObject = pAudioObject;
							pEventInstance->channels.insert(nChannelID);
						}
						else
						{
							g_SDLMixerImplLogger.Log(eALT_ERROR, "Could not play sample. Error: %s", Mix_GetError());
						}
					}
					else
					{
						g_SDLMixerImplLogger.Log(eALT_ERROR, "Ran out of free audio channels. Are you trying to play more than %d samples?", SDL_MIXER_NUM_CHANNELS);
					}

					if (!pEventInstance->channels.empty())
					{
						// if any sample was added then add the event to the audio object
						pAudioObject->events.insert(pEventInstance);
						bSuccess = true;
					}
				}
				else // stop event in audio object
				{
					TEventInstanceSet::const_iterator eventIt = pAudioObject->events.begin();
					TEventInstanceSet::const_iterator eventEnd = pAudioObject->events.end();
					for (; eventIt != eventEnd; ++eventIt)
					{
						SSDLMixerEventInstanceData* pEventInstance = *eventIt;
						if (pEventInstance)
						{
							if (pEventInstance->pStaticData->nSampleID == pEventStaticData->nSampleID)
							{
								StopEvent(pEventInstance);
							}
						}
					}
				}
			}

			return bSuccess;
		}

		bool SetListenerPosition(const TListenerID nListenerID, const CAudioObjectTransformation& position)
		{
			g_listenerPosition = position;
			g_bListenerPosChanged = true;
			return true;
		}

		bool RegisterAudioObject(SSDLMixerAudioObjectData* pAudioObjectData)
		{
			if (pAudioObjectData)
			{
				g_audioObjects.push_back(pAudioObjectData);
				return true;
			}
			return false;
		}

		bool UnregisterAudioObject(SSDLMixerAudioObjectData* pAudioObjectData)
		{
			if (pAudioObjectData)
			{
				stl::find_and_erase(g_audioObjects, pAudioObjectData);
				return true;
			}
			return false;
		}

		bool SetAudioObjectPosition(SSDLMixerAudioObjectData* pAudioObjectData, const CAudioObjectTransformation& position)
		{
			if (pAudioObjectData)
			{
				pAudioObjectData->position = position;
				pAudioObjectData->bPositionChanged = true;
				return true;
			}
			return false;
		}

		bool StopEvent(SSDLMixerEventInstanceData const* const pEventInstance)
		{
			if (pEventInstance)
			{
				// need to make a copy because the callback
				// registered with Mix_ChannelFinished can edit the list
				TChannelSet channels = pEventInstance->channels;
				TChannelSet::const_iterator channelIt = channels.begin();
				TChannelSet::const_iterator channelEnd = channels.end();
				for (; channelIt != channelEnd; ++channelIt)
				{
					Mix_HaltChannel(*channelIt);
				}
				return true;
			}
			return false;
		}


		bool StopEvents(const SDLMixer::TEventID nEventID)
		{
			bool bResult = false;
			TAudioObjectList::const_iterator audioObjectIt = g_audioObjects.begin();
			const TAudioObjectList::const_iterator audioObjectEnd = g_audioObjects.end();
			for (; audioObjectIt != audioObjectEnd; ++audioObjectIt)
			{
				SSDLMixerAudioObjectData* pAudioObject = *audioObjectIt;
				if (pAudioObject)
				{
					TEventInstanceSet::const_iterator eventIt = pAudioObject->events.begin();
					const TEventInstanceSet::const_iterator eventEnd = pAudioObject->events.end();
					for (; eventIt != eventEnd; ++eventIt)
					{
						SSDLMixerEventInstanceData* pEventInstance = *eventIt;
						if (pEventInstance && pEventInstance->pStaticData && pEventInstance->pStaticData->nSDLMixerEventID == nEventID)
						{
							StopEvent(pEventInstance);
							bResult = true;
						}
					}
				}
			}
			return bResult;
		}

		SSDLMixerEventStaticData* CreateEventData(const TEventID nEventID)
		{
			SSDLMixerEventStaticData* pNewTriggerImpl = nullptr;
			if (nEventID != SDLMixer::SDL_MIXER_INVALID_EVENT_ID)
			{
				POOL_NEW(SSDLMixerEventStaticData, pNewTriggerImpl)(nEventID);
			}
			else
			{
				assert(false);
			}
			return pNewTriggerImpl;
		}

		void Update()
		{
			ProcessChannelFinishedRequests(g_channelFinishedRequests[eCFRQID_TWO]);
			{
				CryAutoLock<CryCriticalSection> oAutoLock(g_channelFinishedCriticalSection);
				g_channelFinishedRequests[eCFRQID_ONE].swap(g_channelFinishedRequests[eCFRQID_TWO]);
			}

			TAudioObjectList::const_iterator audioObjectIt = g_audioObjects.begin();
			const TAudioObjectList::const_iterator audioObjectEnd = g_audioObjects.end();
			for (; audioObjectIt != audioObjectEnd; ++audioObjectIt)
			{
				SSDLMixerAudioObjectData* pAudioObject = *audioObjectIt;
				if (pAudioObject && (pAudioObject->bPositionChanged || g_bListenerPosChanged))
				{
					// Get distance and angle from the listener to the audio object
					float fDistance = 0.0f;
					float fAngle = 0.0f;
					GetDistanceAngleToObject(g_listenerPosition, pAudioObject->position, fDistance, fAngle);
					const uint8 nSDLMaxDistance = 255;

					TEventInstanceSet::const_iterator eventIt = pAudioObject->events.begin();
					const TEventInstanceSet::const_iterator eventEnd = pAudioObject->events.end();
					for (; eventIt != eventEnd; ++eventIt)
					{
						SSDLMixerEventInstanceData* pEventInstance = *eventIt;
						if (pEventInstance && pEventInstance->pStaticData)
						{
							TChannelSet::const_iterator channelIt = pEventInstance->channels.begin();
							const TChannelSet::const_iterator channelEnd = pEventInstance->channels.end();
							for (; channelIt != channelEnd; ++channelIt)
							{
								SetChannelPosition(pEventInstance, *channelIt, fDistance, fAngle);
							}
						}
					}
					pAudioObject->bPositionChanged = false;
				}
			}
			g_bListenerPosChanged = false;
		}
	}
}
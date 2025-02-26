// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#include "stdafx.h"
#include "EntityAudioProxy.h"
#include <IAudioSystem.h>
#include <ICryAnimation.h>
#include "Entity.h"

CEntityAudioProxy::TAudioProxyPair CEntityAudioProxy::s_oNULLAudioProxyPair(INVALID_AUDIO_PROXY_ID, static_cast<IAudioProxy*>(nullptr));

//////////////////////////////////////////////////////////////////////////
CEntityAudioProxy::CEntityAudioProxy()
	:	m_pEntity(nullptr)
	,	m_nAudioProxyIDCounter(INVALID_AUDIO_PROXY_ID)
	,	m_nAudioEnvironmentID(INVALID_AUDIO_ENVIRONMENT_ID)
	,	m_nFlags(eEAPF_CAN_MOVE_WITH_ENTITY)
	,	m_fFadeDistance(0.0f)
	,	m_fEnvironmentFadeDistance(0.0f)
{
}

//////////////////////////////////////////////////////////////////////////
CEntityAudioProxy::~CEntityAudioProxy()
{
	m_pEntity = nullptr;
	std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SReleaseAudioProxy());
	m_mapAuxAudioProxies.clear();
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::Initialize(SComponentInitializer const& init)
{
	m_pEntity = static_cast<CEntity*>(init.m_pEntity);
	assert(m_mapAuxAudioProxies.empty());

	// Creating the default AudioProxy.
	CreateAuxAudioProxy();
	SetObstructionCalcType(eAOOCT_Ignore);
	OnMove();
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::Reload(IEntity* pEntity, SEntitySpawnParams& params)
{
	m_pEntity										= static_cast<CEntity*>(pEntity);
	m_fFadeDistance							= 0.0f;
	m_fEnvironmentFadeDistance	= 0.0f;
	m_nAudioEnvironmentID				= INVALID_AUDIO_ENVIRONMENT_ID;

	std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SResetAudioProxy());

#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
	std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SInitializeAudioProxy(m_pEntity->GetName()));
#else
	std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SInitializeAudioProxy(nullptr));
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

	SetObstructionCalcType(eAOOCT_Ignore);
	OnMove();
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnMove()
{
	const Matrix34& tm = m_pEntity->GetWorldTM();

	if (tm.IsValid())
	{
		if ((m_nFlags & eEAPF_CAN_MOVE_WITH_ENTITY) != 0)
		{
			std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SRepositionAudioProxy(tm));
		}

		if ((m_pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0)
		{
			SAudioRequest oRequest;
			oRequest.nFlags = eARF_PRIORITY_NORMAL;
			oRequest.pOwner = this;

			Matrix34 position = tm;
			position += CVar::audioListenerOffset;
			SAudioListenerRequestData<eALRT_SET_POSITION> oRequestData(position);

			oRequest.pData = &oRequestData;

			gEnv->pAudioSystem->PushRequest(oRequest);

			// As this is an audio listener add its entity to the AreaManager for raising audio relevant events.
			gEnv->pEntitySystem->GetAreaManager()->MarkEntityForUpdate(m_pEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnListenerMoveInside(IEntity const* const pEntity)
{
	m_pEntity->SetPos(pEntity->GetWorldPos());
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnMoveInside(IEntity const* const pEntity)
{
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnListenerExclusiveMoveInside(IEntity const* const __restrict pEntity, IEntity const* const __restrict pAreaHigh, IEntity const* const __restrict pAreaLow, float const fFade)
{
	IEntityAreaProxy const* const __restrict pAreaProxyLow	= static_cast<IEntityAreaProxy const* const __restrict>(pAreaLow->GetProxy(ENTITY_PROXY_AREA));
	IEntityAreaProxy* const __restrict pAreaProxyHigh				= static_cast<IEntityAreaProxy* const __restrict>(pAreaHigh->GetProxy(ENTITY_PROXY_AREA));

	if (pAreaProxyLow != nullptr && pAreaProxyHigh != nullptr)
	{
		Vec3 OnHighHull3d(ZERO);
		Vec3 const oPos(pEntity->GetWorldPos());
		EntityId const nEntityID = pEntity->GetId();
		bool const bInsideLow = pAreaProxyLow->CalcPointWithin(nEntityID, oPos);

		if (bInsideLow)
		{
			pAreaProxyHigh->ClosestPointOnHullDistSq(nEntityID, oPos, OnHighHull3d);
			m_pEntity->SetPos(OnHighHull3d);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnExclusiveMoveInside(IEntity const* const __restrict pEntity, IEntity const* const __restrict pEntityAreaHigh, IEntity const* const __restrict pEntityAreaLow, float const fEnvironmentFade)
{
	SetEnvironmentAmountInternal(pEntity, fEnvironmentFade);
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnListenerEnter(IEntity const* const pEntity)
{
	m_pEntity->SetPos(pEntity->GetWorldPos());
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnEnter(IEntity const* const pEntity)
{
	SetEnvironmentAmountInternal(pEntity, 1.0f);
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnLeaveNear(IEntity const* const pEntity)
{
	SetEnvironmentAmountInternal(pEntity, 0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnAreaCrossing()
{
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnListenerMoveNear(IEntity const* const __restrict pEntity, IEntity const* const __restrict pArea)
{
	IEntityAreaProxy* const pAreaProxy = static_cast<IEntityAreaProxy*>(pArea->GetProxy(ENTITY_PROXY_AREA));

	if (pAreaProxy != nullptr)
	{
		Vec3 OnHull3d(ZERO);
		pAreaProxy->CalcPointNearDistSq(pEntity->GetId(), pEntity->GetWorldPos(), OnHull3d);
		m_pEntity->SetPos(OnHull3d);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::OnMoveNear(IEntity const* const __restrict pEntity, IEntity const* const __restrict pArea)
{
	IEntityAudioProxy* const pIEntityAudioProxy = static_cast<IEntityAudioProxy*>(pEntity->GetProxy(ENTITY_PROXY_AUDIO));
	IEntityAreaProxy* const pAreaProxy = static_cast<IEntityAreaProxy*>(pArea->GetProxy(ENTITY_PROXY_AREA));

	if ((pIEntityAudioProxy != nullptr) && (pAreaProxy != nullptr) && (m_nAudioEnvironmentID != INVALID_AUDIO_ENVIRONMENT_ID))
	{
		// Only set the Audio Environment Amount on the entities that already have an AudioProxy.
		// Passing INVALID_AUDIO_PROXY_ID to address all auxiliary AudioProxies on pEntityAudioProxy.
		Vec3 OnHull3d(ZERO);
		float const fDist = sqrt(pAreaProxy->CalcPointNearDistSq(pEntity->GetId(), pEntity->GetWorldPos(), OnHull3d));

		if ((fDist > m_fEnvironmentFadeDistance) || (m_fEnvironmentFadeDistance < 0.0001f))
		{
			pIEntityAudioProxy->SetEnvironmentAmount(m_nAudioEnvironmentID, 0.0f, INVALID_AUDIO_PROXY_ID);
		}
		else
		{
			pIEntityAudioProxy->SetEnvironmentAmount(m_nAudioEnvironmentID, 1.0f - (fDist/m_fEnvironmentFadeDistance), INVALID_AUDIO_PROXY_ID);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::ProcessEvent( SEntityEvent &event )
{
	if (m_pEntity != nullptr)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_XFORM:
			{
				int const nFlags = (int)event.nParam[0];

				if ((nFlags & (ENTITY_XFORM_POS | ENTITY_XFORM_ROT)) != 0)
				{
					OnMove();
				}

				break;
			}
		case ENTITY_EVENT_ENTERAREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) != 0)
				{
					EntityId const nEntityID = static_cast<EntityId>(event.nParam[0]); // Entering entity!
					IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(nEntityID);

					if ((pEntity != nullptr) && (pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0)
					{
						OnListenerEnter(pEntity);
					}
					else if ((pEntity != nullptr) && (m_nAudioEnvironmentID != INVALID_AUDIO_ENVIRONMENT_ID))
					{
						OnEnter(pEntity);
					}
				}

				break;
			}
		case ENTITY_EVENT_LEAVEAREA:
			{

				break;
			}
		case ENTITY_EVENT_CROSS_AREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) != 0)
				{
					EntityId const nEntityID = static_cast<EntityId>(event.nParam[0]); // Crossing entity!
					IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(nEntityID);

					if ((pEntity != nullptr) && (pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0)
					{
						OnAreaCrossing();
					}
				}

				break;
			}
		case ENTITY_EVENT_MOVENEARAREA:
		case ENTITY_EVENT_ENTERNEARAREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) != 0)
				{
					EntityId const nEntityID = static_cast<EntityId>(event.nParam[0]); // Near entering/moving entity!
					IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(nEntityID);

					EntityId const nAreaEntityID = (EntityId)event.nParam[2];
					IEntity* const pArea = gEnv->pEntitySystem->GetEntity(nAreaEntityID);

					if ((pEntity != nullptr) && (pArea != nullptr))
					{
						if ((pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0)
						{
							// This entity is an audio listener.
							OnListenerMoveNear(pEntity, pArea);
						}
						else if (m_nAudioEnvironmentID != INVALID_AUDIO_ENVIRONMENT_ID)
						{
							OnMoveNear(pEntity, pArea);
						}
					}
				}

				break;
			}
		case ENTITY_EVENT_LEAVENEARAREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) != 0)
				{
					EntityId const nEntityID = static_cast<EntityId>(event.nParam[0]); // Leaving entity!
					IEntity* const pEntity = gEnv->pEntitySystem->GetEntity(nEntityID);

					if ((pEntity != nullptr) && (pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) == 0)
					{
						OnLeaveNear(pEntity);
					}
				}
				break;
			}
		case ENTITY_EVENT_MOVEINSIDEAREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) != 0)
				{
					EntityId const nEntityID = static_cast<EntityId>(event.nParam[0]); // Inside moving entity!
					IEntity* const __restrict pEntity = gEnv->pEntitySystem->GetEntity(nEntityID);

					if (pEntity != nullptr)
					{
						EntityId const nAreaID1 = static_cast<EntityId>(event.nParam[2]); // AreaEntityID (low)
						EntityId const nAreaID2 = static_cast<EntityId>(event.nParam[3]); // AreaEntityID (high)

						IEntity* const __restrict pArea1 = gEnv->pEntitySystem->GetEntity(nAreaID1);
						IEntity* const __restrict pArea2 = gEnv->pEntitySystem->GetEntity(nAreaID2);


						if (pArea1 != nullptr)
						{
							if (pArea2 != nullptr)
							{
								if ((pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0)
								{
									OnListenerExclusiveMoveInside(pEntity, pArea2, pArea1, event.fParam[0]);
								}
								else
								{
									OnExclusiveMoveInside(pEntity, pArea2, pArea1, event.fParam[1]);
								}
							}
							else
							{
								if ((pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) != 0)
								{
									OnListenerMoveInside(pEntity);
								}
								else
								{
									OnMoveInside(pEntity);
								}
							}
						}
					}
				}

				break;
			}
		case ENTITY_EVENT_ANIM_EVENT:
			{
				REINST("reintroduce anim event voice playing in EntityAudioProxy");
				/*if (!IsSoundAnimEventsHandledExternally())
				{
					const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
					ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
					const char* eventName = (pAnimEvent ? pAnimEvent->m_EventName : 0);
					if (eventName && stricmp(eventName, "sound") == 0)
					{
						Vec3 offset(ZERO);
						if (pAnimEvent->m_BonePathName && pAnimEvent->m_BonePathName[0])
						{
							if (pCharacter)
							{
								IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
								int id = rIDefaultSkeleton.GetJointIDByName(pAnimEvent->m_BonePathName);
								if (id >= 0)
								{
									ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
									QuatT boneQuat(pSkeletonPose->GetAbsJointByID(id));
									offset = boneQuat.t;
								}
							}
						}

						int flags = FLAG_SOUND_DEFAULT_3D;
						if (strchr(pAnimEvent->m_CustomParameter, ':') == nullptr)
							flags |= FLAG_SOUND_VOICE;
						PlaySound(pAnimEvent->m_CustomParameter, offset, FORWARD_DIRECTION, flags, 0, eSoundSemantic_Animation, 0, 0);
					}
				}*/

				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityAudioProxy::GetSignature(TSerialize signature)
{
	// EntityAudioProxy is not relevant to signature as it is always created again if needed
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::Serialize(TSerialize ser)
{
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::PlayFile(
	char const* const szFile,
	TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */,
	SAudioCallBackInfos const& callBackInfos /* = SAudioCallBackInfos::GetEmptyObject() */)
{
	if (m_pEntity != nullptr)
	{
		if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
		{
			TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

			if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
			{
				(SPlayFile(szFile, callBackInfos))(rAudioProxyPair);
			}
		}
		else
		{
			std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SPlayFile(szFile, callBackInfos));
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to play an audio file on an EntityAudioProxy without a valid entity!");
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::StopFile(
	char const* const szFile,
	TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */,
	SAudioCallBackInfos const& callBackInfos /* = SAudioCallBackInfos::GetEmptyObject() */)
{
	if (m_pEntity != nullptr)
	{
		if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
		{
			TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

			if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
			{
				(SStopFile(szFile, callBackInfos))(rAudioProxyPair);
			}
		}
		else
		{
			std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SStopFile(szFile, callBackInfos));
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to stop an audio file on an EntityAudioProxy without a valid entity!");
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityAudioProxy::ExecuteTrigger(
	TAudioControlID const nTriggerID,
	ELipSyncMethod const eLipSyncMethod,
	TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */,
	SAudioCallBackInfos const& callBackInfos /* = SAudioCallBackInfos::GetEmptyObject() */)
{
	if (m_pEntity != nullptr)
	{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
		if (m_pEntity->GetWorldTM().GetTranslation() == Vec3Constants<float>::fVec3_Zero)
		{
			gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to execute an audio trigger at (0,0,0) position in the entity %s. Entity may not be initialized correctly!", m_pEntity->GetEntityTextDescription());
		}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

		if ((m_pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_DISABLED) == 0)
		{
			if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
			{
				TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

				if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
				{
					(SRepositionAudioProxy(m_pEntity->GetWorldTM()))(rAudioProxyPair);
					rAudioProxyPair.second.pIAudioProxy->ExecuteTrigger(nTriggerID, eLipSyncMethod, callBackInfos);
					return true;
				}
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
				else
				{
					gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Could not find AuxAudioProxy with id '%u' on entity '%s' to ExecuteTrigger '%s'", nAudioProxyLocalID, m_pEntity->GetEntityTextDescription(), gEnv->pAudioSystem->GetAudioControlName(eACT_TRIGGER, nTriggerID));
				}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
			}
			else
			{
				for (TAuxAudioProxies::iterator it = m_mapAuxAudioProxies.begin(); it != m_mapAuxAudioProxies.end(); ++it)
				{
					(SRepositionAudioProxy(m_pEntity->GetWorldTM()))(*it);
					it->second.pIAudioProxy->ExecuteTrigger(nTriggerID, eLipSyncMethod, callBackInfos);
				}
				return !m_mapAuxAudioProxies.empty();
			}
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to execute an audio trigger on an EntityAudioProxy without a valid entity!");
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//void CEntityAudioProxy::ExecuteTrigger(TAudioControlID const nTriggerID, ELipSyncMethod const eLipSyncMethod, TTriggerFinishedCallback const pCallback, void* const pCallbackCookie, TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
//{
//	ExecuteTriggerInternal(nTriggerID, eLipSyncMethod, pCallback, pCallbackCookie, nAudioProxyLocalID);
//}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::StopTrigger(TAudioControlID const nTriggerID, TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

		if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SStopTrigger(nTriggerID))(rAudioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SStopTrigger(nTriggerID));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::SetSwitchState(TAudioControlID const nSwitchID, TAudioSwitchStateID const nStateID, TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

		if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SSetSwitchState(nSwitchID, nStateID))(rAudioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetSwitchState(nSwitchID, nStateID));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::SetRtpcValue(TAudioControlID const nRtpcID, float const fValue, TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

		if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SSetRtpcValue(nRtpcID, fValue))(rAudioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetRtpcValue(nRtpcID, fValue));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::SetObstructionCalcType(EAudioObjectObstructionCalcType const eObstructionType, TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

		if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SSetObstructionCalcType(eObstructionType))(rAudioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetObstructionCalcType(eObstructionType));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::SetEnvironmentAmount(TAudioEnvironmentID const nEnvironmentID, float const fAmount, TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

		if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			SSetEnvironmentAmount(nEnvironmentID, fAmount)(rAudioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetEnvironmentAmount(nEnvironmentID, fAmount));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::SetCurrentEnvironments(TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

		if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			SSetCurrentEnvironments(m_pEntity->GetId())(rAudioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetCurrentEnvironments(m_pEntity->GetId()));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::AuxAudioProxiesMoveWithEntity(bool const bCanMoveWithEntity)
{
	if (bCanMoveWithEntity)
	{
		m_nFlags |= eEAPF_CAN_MOVE_WITH_ENTITY;
	}
	else
	{
		m_nFlags &= ~eEAPF_CAN_MOVE_WITH_ENTITY;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::AddAsListenerToAuxAudioProxy(TAudioProxyID const nAudioProxyLocalID, void (*func)(SAudioRequestInfo const* const), EAudioRequestType requestType /* = eART_AUDIO_ALL_REQUESTS */, TATLEnumFlagsType specificRequestMask /* = ALL_AUDIO_REQUEST_SPECIFIC_TYPE_FLAGS */)
{
	TAuxAudioProxies::const_iterator const Iter(m_mapAuxAudioProxies.find(nAudioProxyLocalID));

	if (Iter != m_mapAuxAudioProxies.end())
	{
		gEnv->pAudioSystem->AddRequestListener(func, Iter->second.pIAudioProxy, requestType, specificRequestMask);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::RemoveAsListenerFromAuxAudioProxy(TAudioProxyID const nAudioProxyLocalID, void (*func)(SAudioRequestInfo const* const))
{
	TAuxAudioProxies::const_iterator const Iter(m_mapAuxAudioProxies.find(nAudioProxyLocalID));

	if (Iter != m_mapAuxAudioProxies.end())
	{
		gEnv->pAudioSystem->RemoveRequestListener(func, Iter->second.pIAudioProxy);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::SetAuxAudioProxyOffset(Matrix34 const& rOffset, TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	if (nAudioProxyLocalID != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair& rAudioProxyPair = GetAuxAudioProxyPair(nAudioProxyLocalID);

		if (rAudioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			SSetAuxAudioProxyOffset(rOffset, m_pEntity->GetWorldTM())(rAudioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetAuxAudioProxyOffset(rOffset, m_pEntity->GetWorldTM()));
	}
}

//////////////////////////////////////////////////////////////////////////
Matrix34 const& CEntityAudioProxy::GetAuxAudioProxyOffset(TAudioProxyID const nAudioProxyLocalID /* = DEFAULT_AUDIO_PROXY_ID */)
{
	TAuxAudioProxies::const_iterator const Iter(m_mapAuxAudioProxies.find(nAudioProxyLocalID));

	if (Iter != m_mapAuxAudioProxies.end())
	{
		return Iter->second.oOffset;
	}

	static const Matrix34 identityMatrix(IDENTITY);
	return identityMatrix;
}

//////////////////////////////////////////////////////////////////////////
TAudioProxyID CEntityAudioProxy::CreateAuxAudioProxy()
{
	TAudioProxyID nAudioProxyLocalID = INVALID_AUDIO_PROXY_ID;
	IAudioProxy* const pIAudioProxy = gEnv->pAudioSystem->GetFreeAudioProxy();

	if (pIAudioProxy != nullptr)
	{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
		if (m_nAudioProxyIDCounter == std::numeric_limits<TAudioProxyID>::max())
		{
			CryFatalError("<Audio> Exceeded numerical limits during CEntityAudioProxy::CreateAudioProxy!");
		}
		else if (m_pEntity == nullptr)
		{
			CryFatalError("<Audio> nullptr entity pointer during CEntityAudioProxy::CreateAudioProxy!");
		}

		CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> sFinalName(m_pEntity->GetName());
		size_t const nNumAuxAudioProxies = m_mapAuxAudioProxies.size();

		if (nNumAuxAudioProxies > 0)
		{
			// First AuxAudioProxy is not explicitly identified, it keeps the entity's name.
			// All additionally AuxaudioProxies however are being explicitly identified.
			sFinalName.Format("%s_auxaudioproxy_#%" PRISIZE_T, m_pEntity->GetName(), nNumAuxAudioProxies + 1);
		}

		pIAudioProxy->Initialize(sFinalName.c_str());
#else
		pIAudioProxy->Initialize(nullptr);
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

		pIAudioProxy->SetPosition(m_pEntity->GetWorldPos());
		pIAudioProxy->SetObstructionCalcType(eAOOCT_Ignore);
		pIAudioProxy->SetCurrentEnvironments(m_pEntity->GetId());

		m_mapAuxAudioProxies.insert(TAudioProxyPair(++m_nAudioProxyIDCounter, SAudioProxyWrapper(pIAudioProxy)));
		nAudioProxyLocalID = m_nAudioProxyIDCounter;
	}

	return nAudioProxyLocalID;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityAudioProxy::RemoveAuxAudioProxy(TAudioProxyID const nAudioProxyLocalID)
{
	bool bSuccess = false;

	if (nAudioProxyLocalID != DEFAULT_AUDIO_PROXY_ID)
	{
		TAuxAudioProxies::iterator Iter(m_mapAuxAudioProxies.find(nAudioProxyLocalID));

		if (Iter != m_mapAuxAudioProxies.end())
		{
			Iter->second.pIAudioProxy->Release();
			m_mapAuxAudioProxies.erase(Iter);
			bSuccess = true;
		}
		else
		{
			gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> AuxAudioProxy with ID '%u' not found during CEntityAudioProxy::RemoveAuxAudioProxy (%s)!", nAudioProxyLocalID, m_pEntity->GetEntityTextDescription());
			assert(false);
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to remove the default AudioProxy during CEntityAudioProxy::RemoveAuxAudioProxy (%s)!", m_pEntity->GetEntityTextDescription());
		assert(false);
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
CEntityAudioProxy::TAudioProxyPair& CEntityAudioProxy::GetAuxAudioProxyPair(TAudioProxyID const nAudioProxyLocalID)
{
	TAuxAudioProxies::iterator const Iter(m_mapAuxAudioProxies.find(nAudioProxyLocalID));

	if (Iter != m_mapAuxAudioProxies.end())
	{
		return *Iter;
	}

	return s_oNULLAudioProxyPair;
}

//////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::SetEnvironmentAmountInternal(IEntity const* const pEntity, float const fAmount) const
{
	IEntityAudioProxy* const pIEntityAudioProxy = static_cast<IEntityAudioProxy*>(pEntity->GetProxy(ENTITY_PROXY_AUDIO));

	if ((pIEntityAudioProxy != nullptr) && (m_nAudioEnvironmentID != INVALID_AUDIO_ENVIRONMENT_ID))
	{
		// Only set the audio-environment-amount on the entities that already have an AudioProxy.
		// Passing INVALID_AUDIO_PROXY_ID to address all auxiliary AudioProxies on pEntityAudioProxy.
		pIEntityAudioProxy->SetEnvironmentAmount(m_nAudioEnvironmentID, fAmount, INVALID_AUDIO_PROXY_ID);
	}
}

///////////////////////////////////////////////////////////////////////////
void CEntityAudioProxy::Done()
{
	m_pEntity = nullptr;
}

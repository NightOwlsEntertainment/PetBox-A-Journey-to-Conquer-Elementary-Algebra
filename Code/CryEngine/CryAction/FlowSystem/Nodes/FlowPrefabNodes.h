////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2012.
// -------------------------------------------------------------------------
//  File name:   FlowPrefabNodes.h
//  Version:     v1.00
//  Created:     11/6/2012 by Dean Claassen
//  Compilers:   Visual Studio.NET 2010
//  Description: Prefab flownode functionality
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FlowPrefabNodes_H__
#define __FlowPrefabNodes_H__
#pragma once

#include "FlowBaseNode.h"
#include <ICustomEvents.h>

//////////////////////////////////////////////////////////////////////////
// Prefab:EventSource node
// This node is placed inside a prefab flowgraph to create / handle a custom event
//////////////////////////////////////////////////////////////////////////
class CFlowNode_PrefabEventSource : public CFlowBaseNode<eNCT_Instanced>, public ICustomEventListener
{
public:
	enum INPUTS
	{
		EIP_PrefabName = 0,
		EIP_InstanceName,
		EIP_EventName,
		EIP_FireEvent,
		EIP_EventId,
		EIP_EventIndex,
	};

	enum OUTPUTS
	{
		EOP_EventFired = 0,
	};

	CFlowNode_PrefabEventSource( SActivationInfo * pActInfo )
	: m_eventId( CUSTOMEVENTID_INVALID )
	{
	}

	~CFlowNode_PrefabEventSource();

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone( SActivationInfo * pActInfo) { return new CFlowNode_PrefabEventSource( pActInfo ); }

	virtual void GetConfiguration( SFlowNodeConfig& config );
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo );

	// ICustomEventListener
	virtual void OnCustomEvent( const TCustomEventId eventId, const TFlowInputData& customData ) override;
	// ~ICustomEventListener

private:
	SActivationInfo m_actInfo;
	TCustomEventId m_eventId;
};

//////////////////////////////////////////////////////////////////////////
// Prefab:Instance node
// This node represents a prefab instance (Currently used to handle events specific to a prefab instance)
//////////////////////////////////////////////////////////////////////////
class CFlowNode_PrefabInstance : public CFlowBaseNode<eNCT_Instanced>, public ICustomEventListener
{
public:
	enum { ePrefabInstance_NumInputPortsPerEvent = 3 };
	enum { ePrefabInstance_NumOutputPortsPerEvent = 1 };

	enum INPUTS
	{
		EIP_PrefabName = 0,
		EIP_InstanceName,
		EIP_Event1,
		EIP_Event1Id,
		EIP_Event1Name,
		// ... reserved ports
		EIP_EventLastPort = EIP_Event1 + (CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE * ePrefabInstance_NumInputPortsPerEvent),
	};

	enum OUTPUTS
	{
		EOP_Event1 = 0,
		// ... reserved ports
		EOP_EventLastPort = EOP_Event1 + (CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE * ePrefabInstance_NumOutputPortsPerEvent),
	};

	CFlowNode_PrefabInstance( SActivationInfo * pActInfo );
	~CFlowNode_PrefabInstance();

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone( SActivationInfo * pActInfo) { return new CFlowNode_PrefabInstance( pActInfo ); }

	virtual void GetConfiguration( SFlowNodeConfig& config );
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo );

	// ICustomEventListener
	virtual void OnCustomEvent( const TCustomEventId eventId, const TFlowInputData& customData ) override;
	// ~ICustomEventListener

private:
	CryFixedArray<TCustomEventId, CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE> m_eventIds;
	SActivationInfo m_actInfo;
};

#endif // __FlowPrefabNodes_H__

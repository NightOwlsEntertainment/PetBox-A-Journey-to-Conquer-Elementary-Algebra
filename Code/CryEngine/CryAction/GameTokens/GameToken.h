////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   GameToken.h
//  Version:     v1.00
//  Created:     20/10/2005 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _GameToken_h_
#define _GameToken_h_
#pragma once

#include "IGameTokens.h"

class CGameTokenSystem;

//////////////////////////////////////////////////////////////////////////
class CGameToken : public IGameToken
{
public:
	CGameToken();
	~CGameToken();

	//////////////////////////////////////////////////////////////////////////
	// IGameToken implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetName( const char *sName );
	virtual const char* GetName() const { return m_name; }
	virtual void SetFlags( uint32 flags ) { m_nFlags = flags; }
	virtual uint32 GetFlags() const { return m_nFlags; }
	virtual EFlowDataTypes GetType() const { return (EFlowDataTypes)m_value.GetType(); };
	virtual void SetType( EFlowDataTypes dataType );
	virtual void SetValue( const TFlowInputData& val );
	virtual bool GetValue( TFlowInputData& val ) const;
	virtual void SetValueAsString( const char* sValue,bool bDefault=false );
	virtual const char* GetValueAsString() const;
	//////////////////////////////////////////////////////////////////////////

	void AddListener( IGameTokenEventListener *pListener ) { stl::push_back_unique(m_listeners,pListener); };
	void RemoveListener( IGameTokenEventListener *pListener ) { stl::find_and_erase(m_listeners,pListener); };
	void Notify( EGameTokenEvent event );

	CTimeValue GetLastChangeTime() const { return m_changed; };

	void GetMemoryStatistics( ICrySizer * s );

private:
	friend class CGameTokenSystem; // Need access to m_name
	static CGameTokenSystem* g_pGameTokenSystem;

	uint32 m_nFlags;
	string m_name;
	TFlowInputData m_value;

	CTimeValue m_changed;

	typedef std::list<IGameTokenEventListener*> Listeneres;
	Listeneres m_listeners; 
};


#endif // _GameToken_h_

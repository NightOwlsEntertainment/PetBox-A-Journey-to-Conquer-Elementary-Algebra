#pragma once

#include "CryUnitTest.h"

struct ILog;
struct IGameplayListener;

namespace CryUnitTest {
	struct IUnitTestManager;
}

struct STimeDemoFrameInfo
{
	int nPolysRendered;
	float fFrameRate;
	int nDrawCalls;
};

struct STimeDemoInfo
{
	STimeDemoInfo():
		nFrameCount(0),pFrames(NULL),lastPlayedTotalTime(0),lastAveFrameRate(0),
		minFPS(0),maxFPS(0),minFPS_Frame(0),maxFPS_Frame(0),
		nTotalPolysRecorded(0),nTotalPolysPlayed(0)
	{

	}
	int nFrameCount;
	STimeDemoFrameInfo *pFrames;

	float lastPlayedTotalTime;
	float lastAveFrameRate;
	float minFPS;
	float maxFPS;
	uint32 minFPS_Frame;
	uint32 maxFPS_Frame;

	// How many polygons per frame where recorded.
	uint32 nTotalPolysRecorded;
	// How many polygons per frame where played.
	uint32 nTotalPolysPlayed;
};

//////////////////////////////////////////////////////////////////////////
// Automatic game testing system.
//////////////////////////////////////////////////////////////////////////
struct ITestSystem
{
	// <interfuscator:shuffle>
	virtual ~ITestSystem(){}
	// can be called through console e.g. #System.ApplicationTest("testcase0")
	// Arguments:
	//   szParam - must not be 0
	virtual void ApplicationTest( const char *szParam )=0;
	// should be called every system update
	virtual void Update()=0;
	//
	virtual void BeforeRender()=0;
	//
	virtual void AfterRender()=0;
	//
	virtual ILog *GetILog()=0;

	// to free the system (not reference counted)
	virtual void Release()=0;

	// Arguments:
	//   fInNSeconds <=0 to deactivate
	virtual void QuitInNSeconds( const float fInNSeconds )=0;

	// Set info about time demo (called by time demo system).
	virtual void SetTimeDemoInfo( STimeDemoInfo *pTimeDemoInfo ) = 0;
	// Retrieve info about last played time demo (return NULL if no time demo info available).
	virtual STimeDemoInfo* GetTimeDemoInfo() = 0;

	virtual CryUnitTest::IUnitTestManager* GetIUnitTestManager() = 0;
	// </interfuscator:shuffle>
};

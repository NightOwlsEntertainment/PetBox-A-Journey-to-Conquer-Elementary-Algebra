////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2003.
// -------------------------------------------------------------------------
//  File name:   IMiniLog.h
//  Version:     v1.00
//  Created:     03/6/2003 by Sergiy.
//  Compilers:   Visual Studio.NET
//  Description: This is the smallest possible interface to the Log - 
//               it's independent and small, so that it can be easily moved
//               across the engine and test applications, to test engine
//               parts that need logging (e.g. Streaming Engine) separately
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _CRY_ENGINE_MINI_LOG_HDR_
#define _CRY_ENGINE_MINI_LOG_HDR_

struct IMiniLog
{
	enum ELogType
	{
		eMessage,
		eWarning,
		eError,
		eAlways,
		eWarningAlways,
		eErrorAlways,
		eInput,						// e.g. "e_CaptureFolder ?" or all printouts from history or auto completion
		eInputResponse,				// e.g. "Set output folder for video capturing" in response to "e_CaptureFolder ?"
		eComment
	};

	// <interfuscator:shuffle>
	// Notes:
	//	 You only have to implement this function.
	virtual void LogV (const ELogType nType, const char* szFormat, va_list args) PRINTF_PARAMS(3, 0) = 0;

	virtual void LogV (const ELogType nType, int flags, const char* szFormat, va_list args) PRINTF_PARAMS(4, 0) = 0;
	
	// Summary:
	//	 Logs using type.
	inline void LogWithType( ELogType nType, const char * szFormat,...) PRINTF_PARAMS(3, 4);

	inline void LogWithType( ELogType nType, int flags, const char * szFormat,...) PRINTF_PARAMS(4, 5);

	// --------------------------------------------------------------------------

	// Summary:
	//	 Destructor.
	virtual ~IMiniLog() {}

	// Description:
	//	 This is the simplest log function for messages
	//	 with the default implementation.
	virtual inline void Log(const char * szFormat,...) PRINTF_PARAMS(2, 3);

	// Description:
	//	 This is the simplest log function for warnings
	//	 with the default implementation.
	virtual inline void LogWarning(const char * szFormat,...) PRINTF_PARAMS(2, 3);

	// Description:
	//	 This is the simplest log function for errors
	//	 with the default implementation.
	virtual inline void LogError(const char * szFormat,...) PRINTF_PARAMS(2, 3);
	// </interfuscator:shuffle>
};

inline void IMiniLog::LogWithType( ELogType nType, int flags, const char * szFormat,...)
{
	va_list args;
	va_start(args,szFormat);
	LogV(nType, flags, szFormat, args);
	va_end(args);
}

inline void IMiniLog::LogWithType( ELogType nType, const char * szFormat,...)
{
	va_list args;
	va_start(args,szFormat);
	LogV(nType, szFormat, args);
	va_end(args);
}

inline void IMiniLog::Log(const char * szFormat,...)
{
	va_list args;
	va_start(args,szFormat);
	LogV (eMessage, szFormat, args);
	va_end(args);
}

inline void IMiniLog::LogWarning(const char * szFormat,...)
{
	va_list args;
	va_start(args,szFormat);
	LogV (eWarning, szFormat, args);
	va_end(args);
}

inline void IMiniLog::LogError(const char * szFormat,...)
{
	va_list args;
	va_start(args,szFormat);
	LogV (eError, szFormat, args);
	va_end(args);
}

// Notes:
//	 By default, to make it possible not to implement the log at the beginning at all,
//	 empty implementations of the two functions are given.
struct CNullMiniLog: public IMiniLog
{
	// Notes:
	//	 The default implementation just won't do anything
	//##@{
	void LogV(const char* szFormat, va_list args) {}
	void LogV(const ELogType nType, const char* szFormat, va_list args) {}
	void LogV (const ELogType nType, int flags, const char* szFormat, va_list args) {}
	//##@}
};


#endif //_CRY_ENGINE_MINI_LOG_HDR_

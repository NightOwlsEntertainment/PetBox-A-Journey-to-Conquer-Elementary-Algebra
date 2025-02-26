/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
    Assert dialog box for Linux. The linux assert dialog is based on a
    small ncurses application which writes the choice to a file. This
    was chosen since there is no default UI system on Linux. X11 wasn't
    used due to the possibility of the system running another display
    protocol (e.g.: WayLand, Mir)

-------------------------------------------------------------------------
History:
- 27:01:2014: Created by Leander Beernaert
*************************************************************************/

#if defined(USE_CRY_ASSERT) && CRY_PLATFORM_LINUX

static char gs_szMessage[MAX_PATH];

void CryAssertTrace(const char * szFormat,...)
{
    if (gEnv == 0)
    {
        return;
    }

    if (!gEnv->bIgnoreAllAsserts || gEnv->bTesting)
    {
        if(szFormat == NULL)
        {
            gs_szMessage[0] = '\0';
        }
        else
        {
            va_list args;
            va_start(args,szFormat);
            vsnprintf(gs_szMessage, sizeof(gs_szMessage), szFormat, args);
            va_end(args);
        }
    }
}

bool CryAssert(const char* szCondition, const char* szFile, unsigned int line, bool *pIgnore)
{
    if (!gEnv) return false;
    static const int max_len = 4096;
    static char gs_command_str[4096];
    static CryLockT<CRYLOCK_RECURSIVE> lock;

    gEnv->pSystem->OnAssert(szCondition, gs_szMessage, szFile, line);

    size_t file_len = strlen(szFile);

    if (!gEnv->bNoAssertDialog && !gEnv->bIgnoreAllAsserts)
    {

        CryAutoLock< CryLockT<CRYLOCK_RECURSIVE> > lk (lock);
        snprintf(gs_command_str, max_len, "xterm -geometry 100x20 -n 'Assert Dialog [Linux Launcher]' -T 'Assert Dialog [Linux Launcher]' -e 'BinLinux/assert_term \"%s\" \"%s\" %d \"%s\"; echo \"$?\" > .assert_return'",
                 szCondition, (file_len > 60) ? szFile + (file_len-61): szFile, line, gs_szMessage);
        int ret = system(gs_command_str);
        if (ret != 0)
        {
            CryLogAlways("<Assert> Terminal failed to execute");
            return false;
        }

        FILE* assert_file = fopen(".assert_return", "r");
        if (!assert_file)
        {
            CryLogAlways("<Assert> Couldn't open assert file");
            return false;
        }
        int result = -1;
        fscanf(assert_file, "%d", &result);
        fclose(assert_file);

        switch(result)
        {
            case 0:
                break;
            case 1:
                *pIgnore = true;
                break;
            case 2:
                gEnv->bIgnoreAllAsserts = true;
                break;
            case 3:
                return true;
                break;
            case 4:
                raise(SIGABRT);
                exit(-1);
                break;
            default:
                CryLogAlways("<Assert> Unknown result in assert file: %d", result);
                return false;
        }

    }


    return false;

}

#endif

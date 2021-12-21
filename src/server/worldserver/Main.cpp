/*
 * Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-GPL2
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

/// \addtogroup Trinityd Trinity Daemon
/// @{
/// \file

#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <ace/Version.h>

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Configuration/Config.h"

#include "Log.h"
#include "Master.h"
#include <stdio.h>
#include <tchar.h>
#include <dbghelp.h>
#include "ObjectAccessor.h"
#pragma comment(linker, "/DEFAULTLIB:dbghelp.lib")

#ifndef _ACORE_CORE_CONFIG
#define _ACORE_CORE_CONFIG "worldserver.conf"
#endif

#ifdef _WIN32
#include "ServiceWin32.h"
char serviceName[] = "worldserver";
char serviceLongName[] = "AzerothCore world service";
char serviceDescription[] = "AzerothCore World of Warcraft emulator world service";
/*
 * -1 - not in service mode
 *  0 - stopped
 *  1 - running
 *  2 - paused
 */
int m_ServiceStatus = -1;
#endif

WorldDatabaseWorkerPool WorldDatabase;                      ///< Accessor to the world database
CharacterDatabaseWorkerPool CharacterDatabase;              ///< Accessor to the character database
LoginDatabaseWorkerPool LoginDatabase;                      ///< Accessor to the realm/login database

uint32 realmID;                                             ///< Id of the realm

//增加宕机回调
struct CustomPackage : public SYMBOL_INFO_PACKAGE
{
    CustomPackage()
    {
        si.SizeOfStruct = sizeof(SYMBOL_INFO);
        si.MaxNameLen = sizeof(name);
    }
};

//回调函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS *Exception)
{
    //保存玩家
    sObjectAccessor->SaveAllPlayers();

    //开始记录宕机日志
    SymSetOptions(SYMOPT_DEFERRED_LOADS);

    if (!SymInitialize(GetCurrentProcess(), 0, TRUE))
        ::TerminateProcess(GetCurrentProcess(), 4);

    PCONTEXT ExcContext = Exception->ContextRecord;
    CONTEXT *MainContext = ExcContext;

    STACKFRAME64 StackFrame;
    memset(&StackFrame, 0, sizeof(StackFrame));

    StackFrame.AddrPC.Offset = MainContext->Rip;
    StackFrame.AddrPC.Mode = AddrModeFlat;
    StackFrame.AddrStack.Offset = MainContext->Rsp;
    StackFrame.AddrStack.Mode = AddrModeFlat;
    StackFrame.AddrFrame.Offset = MainContext->Rbp;
    StackFrame.AddrFrame.Mode = AddrModeFlat;

    TCHAR ModuleFolderName[MAX_PATH];
    GetModuleFileName(0, ModuleFolderName, MAX_PATH);
    TCHAR* AppFolder = _tcsrchr(ModuleFolderName, '\\');
    if (!AppFolder)
        ::TerminateProcess(GetCurrentProcess(), 4);

    AppFolder[0] = '\0';
    ++AppFolder;

    //保存宕机日志的文件夹
    TCHAR CrashFolderPath[MAX_PATH];
    sprintf(CrashFolderPath, "%s\\%s", ModuleFolderName, _T("FrozenCore-Crashes"));

    if (!CreateDirectoryA(CrashFolderPath, NULL))
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            ::TerminateProcess(GetCurrentProcess(), 4);
    }

    SYSTEMTIME DateSystime;
    GetLocalTime(&DateSystime);
    //生成DMP调试文件
    TCHAR m_szDumpFileName[MAX_PATH];
    sprintf(m_szDumpFileName, "%s\\%s[%u月%u日%u时%u分%u秒].dmp",
        CrashFolderPath, AppFolder, DateSystime.wMonth, DateSystime.wDay, DateSystime.wHour, DateSystime.wMinute, DateSystime.wSecond);

    HANDLE m_hDumpFile = CreateFile(m_szDumpFileName,
        GENERIC_WRITE,
        0,
        0,
        OPEN_ALWAYS,
        FILE_FLAG_WRITE_THROUGH,
        0);

    if (m_hDumpFile)
    {
        MINIDUMP_EXCEPTION_INFORMATION info;
        info.ClientPointers = FALSE;
        info.ExceptionPointers = Exception;
        info.ThreadId = GetCurrentThreadId();

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
            m_hDumpFile, MiniDumpWithIndirectlyReferencedMemory, &info, 0, 0);

        CloseHandle(m_hDumpFile);
    }

    //格式化文件名
    char FileName[MAX_PATH];
    snprintf(FileName, MAX_PATH, _T("%s\\%s[%u月%u日%u时%u分%u秒].txt"), CrashFolderPath, AppFolder, DateSystime.wMonth, DateSystime.wDay, DateSystime.wHour, DateSystime.wMinute, DateSystime.wSecond);
    FILE *CrashesFile = fopen(FileName, "w");
    fprintf(CrashesFile, _T("\n"));

    //直到记录完毕后退出
    while (true)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(), &StackFrame, ExcContext, 0, SymFunctionTableAccess64, SymGetModuleBase64, 0))
            break;

        if (0 == StackFrame.AddrFrame.Offset)
            break;

        IMAGEHLP_MODULE64 ModuleInfo;
        memset(&ModuleInfo, 0, sizeof(IMAGEHLP_MODULE64));
        ModuleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

        //得到模块名
        if (SymGetModuleInfo64(GetCurrentProcess(), StackFrame.AddrPC.Offset, &ModuleInfo))
        {
            fprintf(CrashesFile, _T("模块 %s "), ModuleInfo.ModuleName);
        }

        DWORD64 Displacement = 0;
        CustomPackage InfoPackage;

        if (SymFromAddr(GetCurrentProcess(), StackFrame.AddrPC.Offset, &Displacement, &InfoPackage.si))
        {
            fprintf(CrashesFile, _T("关联函数名 %hs "), InfoPackage.si.Name);
        }

        IMAGEHLP_LINE64 LineInfo = { sizeof(IMAGEHLP_LINE64) };
        DWORD LineDisplacement;
        if (SymGetLineFromAddr64(GetCurrentProcess(), StackFrame.AddrPC.Offset, &LineDisplacement, &LineInfo))
        {
            fprintf(CrashesFile, _T("出错代码路径 %s 行号 %u"), LineInfo.FileName, LineInfo.LineNumber);
        }

        fprintf(CrashesFile, "\n");
    }

    fprintf(CrashesFile, _T("-----------------------------------------------------------------------------------------------------"));
    fflush(CrashesFile);
    fclose(CrashesFile);

    //记录宕机日志完成

    ::TerminateProcess(GetCurrentProcess(), 4);

    return EXCEPTION_EXECUTE_HANDLER;
}
/// Print out the usage string for this program on the console.
void usage(const char* prog)
{
    printf("Usage:\n");
    printf(" %s [<options>]\n", prog);
    printf("    -c config_file           use config_file as configuration file\n");
#ifdef _WIN32
    printf("    Running as service functions:\n");
    printf("    --service                run as service\n");
    printf("    -s install               install service\n");
    printf("    -s uninstall             uninstall service\n");
#endif
}

/// Launch the Trinity server
extern int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
    ///- Command line parsing to get the configuration file name
    char const* configFile = _ACORE_CORE_CONFIG;
    int c = 1;
    while (c < argc)
    {
        if (strcmp(argv[c], "--dry-run") == 0)
        {
            sConfigMgr->setDryRun(true);
        }

        if (!strcmp(argv[c], "-c"))
        {
            if (++c >= argc)
            {
                printf("Runtime-Error: -c option requires an input argument");
                usage(argv[0]);
                return 1;
            }
            else
                configFile = argv[c];
        }

        #ifdef _WIN32
        if (strcmp(argv[c], "-s") == 0) // Services
        {
            if (++c >= argc)
            {
                printf("Runtime-Error: -s option requires an input argument");
                usage(argv[0]);
                return 1;
            }

            if (strcmp(argv[c], "install") == 0)
            {
                if (WinServiceInstall())
                    printf("Installing service\n");
                return 1;
            }
            else if (strcmp(argv[c], "uninstall") == 0)
            {
                if (WinServiceUninstall())
                    printf("Uninstalling service\n");
                return 1;
            }
            else
            {
                printf("Runtime-Error: unsupported option %s", argv[c]);
                usage(argv[0]);
                return 1;
            }
        }

        if (strcmp(argv[c], "--service") == 0)
            WinServiceRun();
        #endif
        ++c;
    }

    sConfigMgr->SetConfigList(std::string(configFile), std::string(CONFIG_FILE_LIST));

    if (!sConfigMgr->LoadAppConfigs())
        return 1;

    sLog->outString("Using configuration file %s.", configFile);
    sLog->outString("Using SSL version: %s (library: %s)", OPENSSL_VERSION_TEXT, SSLeay_version(SSLEAY_VERSION));
    sLog->outString("Using ACE version: %s", ACE_VERSION);

    ///- and run the 'Master'
    /// @todo Why do we need this 'Master'? Can't all of this be in the Main as for Realmd?
    int ret = sMaster->Run();

    // at sMaster return function exist with codes
    // 0 - normal shutdown
    // 1 - shutdown at error
    // 2 - restart command used, this code can be used by restarter for restart Trinityd

    return ret;
}

/// @}

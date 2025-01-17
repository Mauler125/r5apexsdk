#pragma once

DLL_EXPORT void SDK_Init();
DLL_EXPORT void SDK_Shutdown();

void Systems_Init();
void Systems_Shutdown();

void Winsock_Startup();
void Winsock_Shutdown();
void DirtySDK_Startup();
void DirtySDK_Shutdown();
void QuerySystemInfo();

void DetourInit();
void DetourAddress();
void DetourRegister();

extern bool g_bSdkInitialized;
extern bool g_bSdkShutdownInitiatedFromConsoleHandler;

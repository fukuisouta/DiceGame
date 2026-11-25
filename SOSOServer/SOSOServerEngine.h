#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "GameConfig.h"
#include "ServerNetworkManager.h"
#include "PacketDispatcher.h"
#include "RoomManager.h"
#include <memory>
#include <atomic>

class SOSOServerEngine
{
public:
    SOSOServerEngine() = default;
    ~SOSOServerEngine();

    bool Initialize(const ServerNetworkConfig& config = ServerNetworkConfig());
    void Run();
    void Shutdown();

    PacketDispatcher& GetDispatcher()
    {
        return m_dispatcher;
    }

private:
#ifdef _WIN32
    static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);
#else
    static void PosixSignalHandler(int signum);
#endif

    void OnClientConnected(uint32_t clientId);
    void OnClientDisconnected(uint32_t clientId);

private:
    ServerNetworkConfig m_config;
    ServerNetworkManager m_network;
    PacketDispatcher m_dispatcher;
    RoomManager m_roomManager;
    std::atomic<bool> m_isInitialized{ false };
};
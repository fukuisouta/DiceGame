#include "NetworkUtils.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

std::vector<std::string> NetworkUtils::GetLocalIPAddresses()
{
    std::vector<std::string> ipList;

#ifdef _WIN32
    char hostName[256] = { 0 };
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR)
    {
        return ipList;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &result) != 0)
    {
        return ipList;
    }

    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
        char ipStr[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, sizeof(ipStr));
        ipList.push_back(std::string(ipStr));
    }

    freeaddrinfo(result);
#else
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1)
    {
        return ipList;
    }

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char ipStr[INET_ADDRSTRLEN] = { 0 };
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, sizeof(ipStr));
            ipList.push_back(std::string(ipStr));
        }
    }

    freeifaddrs(ifaddr);
#endif

    return ipList;
}

std::string NetworkUtils::GetPrimaryLocalIP()
{
    auto ipList = GetLocalIPAddresses();
    for (const auto& ip : ipList)
    {
        if (ip != "127.0.0.1" && ip.find("169.254.") != 0)
        {
            return ip;
        }
    }
    return "127.0.0.1";
}
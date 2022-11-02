#pragma once

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <vector>
#include <map>
#include <mutex>
#include "Client.h"
#include "Packet.h"
#include "Instruction.h"

#pragma comment(lib, "Ws2_32.lib")

class Serializer;

class Server
{
public:
    Server();
    ~Server();
    void Start();
    void Routine();
    void Accept();
    void Receive();
    void ShutdownConnection(Client Client);
    void RegisterInstruction(NetDataType ID, Instruction Instruction);

    Serializer* GetSerializer();
    std::string GetClientIP(SOCKET ClientSocket, sockaddr_storage* pClientAddress);

private:
    bool m_Shutdown;
    SOCKET m_Socket;
    Serializer* m_pSerializer;
    std::mutex m_Mutex;
    std::map<NetDataType, Instruction> m_Instructions;
    std::vector<Client> m_Clients;
};
#pragma once

#define WIN32_LEAN_AND_MEAN
#define BUFFER_SIZE 512

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <vector>
#include <map>
#include <mutex>
#include "Client.h"
#include "Instruction.h"

#pragma comment(lib, "Ws2_32.lib")

class Serializer;

class Server
{
public:
    Server();
    void Start();
    void HandleReceive(Client Client);
    void Routine();
    void ShutdownConnection(Client Client);
    void RegisterInstruction(int ID, Instruction Instruction);

    Serializer* GetSerializer();
    std::string GetClientIP(SOCKET ClientSocket, sockaddr_storage* pClientAddress);

private:
    SOCKET m_Socket;
    Serializer* m_pSerializer;
    std::mutex m_Mutex;
    std::map<int, Instruction> m_Instructions;
    std::vector<Client> m_Clients;
};
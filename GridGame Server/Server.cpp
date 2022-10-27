#include "Server.h"
#include "DynamicBuffer.h"
#include "Parser.h"
#include "GridGame.h"
#include "Serializer.h"
#include <iostream>
#include <format>

Server::Server()
{
    m_Socket = INVALID_SOCKET;
    m_pSerializer = new Serializer(&m_Instructions);
}

void Server::Start()
{
    WSADATA WSA;
    int Result;
    bool OptVal = false;

    Result = WSAStartup(MAKEWORD(2, 2), &WSA);

    if (Result != NO_ERROR)
        return;

    m_Socket = socket(AF_INET6, SOCK_STREAM, 0);
        
    if (m_Socket == INVALID_SOCKET)
        return;

    Result = setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (char*)&OptVal, sizeof(OptVal));

    if (Result == SOCKET_ERROR)
        return;
    
    ADDRINFOA Info = { 0 };
    Info.ai_family = AF_INET6;
    Info.ai_socktype = SOCK_STREAM;
    Info.ai_protocol = IPPROTO_TCP;

    PADDRINFOA InfoResult;
    Result = getaddrinfo("2a02:8109:9fc0:4394::cd7a", "42694", &Info, &InfoResult);

    if (Result != 0)
        return;

    Result = bind(m_Socket, InfoResult->ai_addr, (int)InfoResult->ai_addrlen);
    freeaddrinfo(InfoResult);

    if (Result == SOCKET_ERROR)
        return;

    Result = listen(m_Socket, 5);

    if (Result == SOCKET_ERROR)
        return;

    std::cout << "Server started, waiting for connections...\n";

    while (true) 
    {
        sockaddr_storage ClientAddress = { 0 };
        int Length = sizeof(ClientAddress);

        SOCKET ClientSocket = accept(m_Socket, (sockaddr*)&ClientAddress, &Length);

        if (ClientSocket == INVALID_SOCKET)
        {
            Sleep(10);
            continue;
        }
        
        Client NewClient(ClientSocket, GetClientIP(ClientSocket, &ClientAddress));
        std::lock_guard LockGuard(m_Mutex);
        std::thread NewThread = std::thread(&Server::HandleReceive, this, NewClient);
        NewThread.detach();

        m_Clients.push_back(NewClient);
    }

    delete m_pSerializer;
}

std::string Server::GetClientIP(SOCKET ClientSocket, sockaddr_storage* pClientAddress)
{
    std::string IP;
    IP.resize(INET6_ADDRSTRLEN);

    int AddressSize = sizeof(sockaddr);

    getpeername(ClientSocket, (sockaddr*)pClientAddress, &AddressSize);

    switch (pClientAddress->ss_family)
    {
    case AF_INET:
        inet_ntop(AF_INET, &(((struct sockaddr_in*)pClientAddress)->sin_addr), IP.data(), IP.length());
        break;

    case AF_INET6:
        inet_ntop(AF_INET6, &(((struct sockaddr_in6*)pClientAddress)->sin6_addr), IP.data(), IP.length());
        break;

    default:
        IP = "UNDEFINED";
        break;
    }

    return IP;
}

void Server::ShutdownConnection(Client Client)
{
    std::lock_guard LockGuard(m_Mutex);

    for (auto It = m_Clients.begin(); It != m_Clients.end(); It++)
    {
        if (*It == Client)
        {
            m_Clients.erase(It);
            break;
        }
    }

    closesocket(Client.m_Socket);
}

void Server::RegisterInstruction(int ID, Instruction Instruction)
{
    m_Instructions[ID] = Instruction;
}

Serializer* Server::GetSerializer()
{
    return m_pSerializer;
}

void Server::HandleReceive(Client Client)
{
    int RecvBytes = 0;
    char TempBuffer[BUFFER_SIZE];
    Serializer Parser(&m_Instructions);
    DynamicBuffer Buffer(BUFFER_SIZE);

    do 
    {
        Serializer::Data Data;
        Serializer::State State = Serializer::State::STATE_DEFAULT;

        // Receive data from stream
        RecvBytes = recv(Client.m_Socket, (char*)&TempBuffer, BUFFER_SIZE, 0);

        // Disconnect
        if (RecvBytes == 0 || RecvBytes < 0)
        {
            g_pGridGame->Disconnect(Client);
            ShutdownConnection(Client);

            return;
        }

        // Write to buffer
        Buffer.Append(TempBuffer, RecvBytes);

        // Deserialize
        while (Buffer.GetSize() > 0 && State != Serializer::State::STATE_INCOMPLETE)
        {
            State = Parser.Deserialize(&Buffer, &Data);

            // Handle data
            switch (State)
            {
            case Serializer::State::STATE_ERROR:
                g_pGridGame->Kick(Client);
                ShutdownConnection(Client);
                return;
            case Serializer::State::STATE_SUCCESS:
                g_pGridGame->Receive(Data, Client); // todo: add callbacks
                break;
            }
        }

    } while (RecvBytes > 0);
}
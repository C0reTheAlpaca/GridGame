#include "Server.h"
#include "DynamicBuffer.h"
#include "Parser.h"
#include "GridGame.h"
#include "Serializer.h"
#include <iostream>
#include <format>

Server::Server()
{
    m_Shutdown = false;
    m_Socket = INVALID_SOCKET;
    m_pSerializer = new Serializer();
    m_pSerializer->SetInstructions(&m_Instructions);
}

Server::~Server()
{
    delete m_pSerializer;
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

    unsigned long Arg = 1;
    ioctlsocket(m_Socket, FIONBIO, &Arg);

    std::cout << "Server started, waiting for connections...\n";

    Routine();
}

void Server::Routine()
{
    while (!m_Shutdown)
    {
        Accept();
        Receive();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (Client Client : m_Clients)
        ShutdownConnection(Client);
    
    WSACleanup();
}

void Server::Accept()
{
    // Only allow 64 connections
    if (m_Clients.size() >= 64)
        return;

    sockaddr_storage ClientAddress = { 0 };
    int Length = sizeof(ClientAddress);

    SOCKET ClientSocket = accept(m_Socket, (sockaddr*)&ClientAddress, &Length);

    if (ClientSocket == 0 || ClientSocket == SOCKET_ERROR)
        return;

    Client NewClient(
        ClientSocket, 
        GetClientIP(ClientSocket, &ClientAddress), 
        &m_Instructions
    );

    // Count concurrent connections of this client
    int ConnectionCount = 0;

    for (Client Client : m_Clients)
    {
        if (Client.m_IP == NewClient.m_IP)
            ConnectionCount++;
    }

    // Only allow 4 concurrent connections per client
    if (ConnectionCount >= 4)
    {
        ShutdownConnection(NewClient);
        return;
    }

    FD_ZERO(&NewClient.m_Set);
    FD_SET(ClientSocket, &NewClient.m_Set);

    m_Clients.push_back(NewClient);
}

void Server::Receive()
{
    char TempBuffer[BUFFER_SIZE];

    for (Client Client : m_Clients)
    {
        if (!FD_ISSET(Client.m_Socket, &Client.m_Set))
            continue;

        // Receive data from stream
        int RecvBytes = recv(Client.m_Socket, (char*)&TempBuffer, BUFFER_SIZE, 0);

        // No data received
        if (RecvBytes < 0)
            continue;

        // Disconnect due to error
        if (RecvBytes == 0)
        {
            g_pGridGame->Disconnect(Client);
            ShutdownConnection(Client);

            return;
        }

        Serializer::State State = Serializer::State::STATE_DEFAULT;

        // Write to buffer
        Client.m_ReceiveBuffer.Append(TempBuffer, RecvBytes);

        // Deserialize
        while (Client.m_ReceiveBuffer.GetSize() > 0 && State != Serializer::State::STATE_INCOMPLETE)
        {
            Packet Packet;
            State = Client.m_Serializer.Deserialize(&Client.m_ReceiveBuffer, &Packet);

            // Handle data
            switch (State)
            {
            case Serializer::State::STATE_ERROR:
                g_pGridGame->Kick(Client);
                ShutdownConnection(Client);
                return;
            case Serializer::State::STATE_SUCCESS:
                g_pGridGame->Receive(Packet, Client); // todo: add callbacks
                break;
            case Serializer::State::STATE_MISSING_INSTRUCTIONS:
                m_Shutdown = true;
                ShutdownConnection(Client);
                break;
            }
        }
    }
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

void Server::RegisterInstruction(NetDataType ID, Instruction Instruction)
{
    m_Instructions[ID] = Instruction;
}

Serializer* Server::GetSerializer()
{
    return m_pSerializer;
}
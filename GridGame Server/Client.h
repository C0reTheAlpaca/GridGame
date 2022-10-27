#pragma once
#include <WinSock2.h>

class Client
{
public:
    Client()
    {
        m_Socket = -1;
        m_IP = "";
    }

    Client(SOCKET Socket, std::string IP)
    {
        m_Socket = Socket;
        m_IP = IP;
    }

    bool operator==(const Client& Client) const
    {
        return m_Socket == Client.m_Socket;
    }

    SOCKET m_Socket;
    std::string m_IP;
};
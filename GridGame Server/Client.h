#pragma once
#include <WinSock2.h>
#include "Packet.h"
#include "Serializer.h"
#include "Instruction.h"
#include "DynamicBuffer.h"

#define BUFFER_SIZE 512

class Client
{
public:
    Client()
    {
        m_Socket = -1;
        m_IP = "";
    }

    Client(SOCKET Socket, std::string IP, std::map<NetDataType, Instruction>* pInstructions)
    {
        m_Socket = Socket;
        m_IP = IP;
        m_Serializer.SetInstructions(pInstructions);
    }

    bool operator==(const Client& Client) const
    {
        return m_Socket == Client.m_Socket;
    }

    std::string m_IP;
    fd_set m_Set;		
    SOCKET m_Socket;
    Serializer m_Serializer;
    DynamicBuffer m_ReceiveBuffer = DynamicBuffer(BUFFER_SIZE);
};
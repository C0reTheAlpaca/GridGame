#pragma once
#include <string>
#include <vector>
#include <variant>

typedef std::variant<int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, bool, double, std::string> PacketData;

enum class NetDataType : uint32_t
{
	NET_UNKNOWN,
	NET_CONNECT,
	NET_CONNECT_ACK,
	NET_LEAVE,
	NET_MOVE,
	NET_END_TURN,
	NET_BROADCAST,
	NET_GAME_DATA,
};

class Packet
{
public:
	Packet()
	{
		m_Magic = NetDataType::NET_UNKNOWN;
	}

	Packet(NetDataType Magic)
	{
		m_Magic = Magic;
	}

	void push_back(PacketData Data)
	{
		m_Data.push_back(Data);
	};

	NetDataType m_Magic;
	std::vector<PacketData> m_Data;
};
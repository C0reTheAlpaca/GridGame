#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <winsock2.h>
#include "Instruction.h"
#include "Packet.h"

class DynamicBuffer;

class Serializer
{
public:
	enum State
	{
		STATE_DEFAULT,
		STATE_SUCCESS,
		STATE_ERROR,
		STATE_MISSING_INSTRUCTIONS,
		STATE_INCOMPLETE
	};

	std::vector<std::size_t> m_DataSizes = {
		sizeof(int8_t),
		sizeof(int16_t),
		sizeof(int32_t),
		sizeof(int64_t),
		sizeof(uint8_t),
		sizeof(uint16_t),
		sizeof(uint32_t),
		sizeof(uint64_t),
		sizeof(bool),
		sizeof(double),
		sizeof(uint32_t),
	};

	Serializer();
	void SetInstructions(const std::map<NetDataType, Instruction>* pInstructions);
	void SerializeSend(Packet Values, SOCKET Socket);
	void PushData(InstructionType Type, Packet* pPacket);
	State Deserialize(DynamicBuffer* pBuffer, Packet* pPacket);

	int8_t DeserializeInt8();
	int16_t DeserializeInt16();
	int32_t DeserializeInt32();
	int64_t DeserializeInt64();
	uint8_t DeserializeUInt8();
	uint16_t DeserializeUInt16();
	uint32_t DeserializeUInt32();
	uint64_t DeserializeUInt64();
	std::string DeserializeString();
	double DeserializeDouble();

	void SerializeInt8(PacketData Value);
	void SerializeInt16(PacketData Value);
	void SerializeInt32(PacketData Value);
	void SerializeInt64(PacketData Value);
	void SerializeUInt8(PacketData Value);
	void SerializeUInt16(PacketData Value);
	void SerializeUInt32(PacketData Value);
	void SerializeUInt64(PacketData Value);
	void SerializeDouble(PacketData Value);
	void SerializeString(PacketData Value);
	void SerializeBool(PacketData Value);

private:
	State m_State;
	char* m_pSerializeData;
	char* m_pSerializePointer;
	char* m_pDeserializePointer;
	char* m_pDeserializeEndPointer;
	const std::map<NetDataType, Instruction>* m_pInstructions;
};


#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <winsock2.h>
#include "Instruction.h"

class DynamicBuffer;
typedef std::variant<int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, bool, double, std::string> Variant;
typedef std::vector<Variant> Packet;

class Serializer
{
public:
	enum State
	{
		STATE_DEFAULT,
		STATE_SUCCESS,
		STATE_ERROR,
		STATE_INCOMPLETE
	};

	struct Data
	{
		int Magic;
		Packet Values;
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

	Serializer(const std::map<int, Instruction> * pInstructions);
	void SerializeSend(int Magic, Packet Values, SOCKET Socket);
	State Deserialize(DynamicBuffer* pBuffer, Data* pData);

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

	void SerializeInt8(Variant Value);
	void SerializeInt16(Variant Value);
	void SerializeInt32(Variant Value);
	void SerializeInt64(Variant Value);
	void SerializeUInt8(Variant Value);
	void SerializeUInt16(Variant Value);
	void SerializeUInt32(Variant Value);
	void SerializeUInt64(Variant Value);
	void SerializeDouble(Variant Value);
	void SerializeString(Variant Value);

private:
	State m_State;
	char* m_pSerializeData;
	char* m_pSerializePointer;
	char* m_pDeserializePointer;
	char* m_pDeserializeEndPointer;
	const std::map<int, Instruction>* m_pInstructions;
};


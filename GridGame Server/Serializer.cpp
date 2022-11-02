#include "Serializer.h"
#include "DynamicBuffer.h"

Serializer::Serializer()
{
	m_State = State::STATE_DEFAULT;
	m_pSerializeData = nullptr;
	m_pSerializePointer = nullptr;
	m_pDeserializePointer = nullptr;
	m_pDeserializeEndPointer = nullptr;
	m_pInstructions = nullptr;
}

void Serializer::SetInstructions(const std::map<NetDataType, Instruction>* pInstructions)
{
	m_pInstructions = pInstructions;
}

void Serializer::SerializeSend(Packet Packet, SOCKET Socket)
{
	if (!m_pInstructions)
		return;

	// Get instruction
	auto InstructionIt = m_pInstructions->find(Packet.m_Magic);

	if (InstructionIt == m_pInstructions->end())
		return;

	Instruction Instruction = InstructionIt->second;

	// Calculate total bytes of packet
	std::size_t TotalPacketBytes = sizeof(Packet.m_Magic);

	for (auto It = Packet.m_Data   .begin(); It != Packet.m_Data.end(); It++)
	{
		std::size_t VariantIndex = It->index();

		// Add bytes of type
		TotalPacketBytes += m_DataSizes[VariantIndex];

		// Add bytes of string
		if ((InstructionType)VariantIndex == InstructionType::TYPE_STRING)
		{
			TotalPacketBytes += std::get<std::string>(*It).length() * sizeof(char);
		}
	}

	m_pSerializeData = (char*)std::malloc(TotalPacketBytes);

	if (!m_pSerializeData)
		return;

	m_pSerializePointer = m_pSerializeData;

	// Serialize magic
	SerializeUInt32(PacketData((uint32_t)Packet.m_Magic));

	// Serialize data
	for (auto It = Packet.m_Data.begin(); It != Packet.m_Data.end(); It++)
	{
		switch ((InstructionType)It->index())
		{
		case InstructionType::TYPE_INT8:   SerializeInt8(*It); break;
		case InstructionType::TYPE_INT16:  SerializeInt16(*It); break;
		case InstructionType::TYPE_INT32:  SerializeInt32(*It); break;
		case InstructionType::TYPE_INT64:  SerializeInt64(*It); break;
		case InstructionType::TYPE_UINT8:  SerializeUInt8(*It); break;
		case InstructionType::TYPE_UINT16: SerializeUInt16(*It); break;
		case InstructionType::TYPE_UINT32: SerializeUInt32(*It); break;
		case InstructionType::TYPE_UINT64: SerializeUInt64(*It); break;
		case InstructionType::TYPE_BOOL:   SerializeBool(*It); break;
		case InstructionType::TYPE_DOUBLE: SerializeUInt64(*It); break;
		case InstructionType::TYPE_STRING: SerializeString(*It); break;
		default:
			return;
		}
	}

	// Send data
	send(Socket, m_pSerializeData, (int)TotalPacketBytes, NULL);

	// Cleanup
	free(m_pSerializeData);
}

Serializer::State Serializer::Deserialize(DynamicBuffer* pBuffer, Packet* pPacket)
{
	if (!m_pInstructions)
	{
		m_State = State::STATE_MISSING_INSTRUCTIONS;
		return m_State;
	}

	m_State = State::STATE_SUCCESS;

	// Check if buffer contains magic
	if (pBuffer->GetSize() < sizeof(pPacket->m_Magic))
	{
		m_State = State::STATE_INCOMPLETE;
		return m_State;
	}

	m_pDeserializePointer = &pBuffer->m_Data.front();
	m_pDeserializeEndPointer = m_pDeserializePointer + pBuffer->GetSize();

	// Get magic
	NetDataType Magic = (NetDataType)DeserializeUInt32();
	pPacket->m_Magic = Magic;

	// Get instruction
	auto InstructionIt = m_pInstructions->find((NetDataType)Magic);

	if (InstructionIt == m_pInstructions->end())
	{
		m_State = State::STATE_ERROR;
		return m_State;
	}

	Instruction Instruction = InstructionIt->second;

	auto Types = m_pInstructions->at((NetDataType)Magic).m_Types;

	for (auto It = Types.begin(); It != Types.end(); It++)
	{
		int Index = (int)std::distance(Types.begin(), It);

		// Check if begin of structure
		if (*It == InstructionType::TYPE_UINT32 &&
			Instruction.m_StructLookup.contains(Index))
		{
			// Deserialize structure size
			uint32_t StructCount = DeserializeUInt32();
			pPacket->m_Data.push_back(StructCount);

			for (uint32_t i = 0; i < StructCount; i++)
			{
				for (InstructionType Type : Instruction.m_StructLookup[Index])
				{
					PushData(*It, pPacket);

					/// Check if data is incomplete
					if (m_State == State::STATE_INCOMPLETE)
						return m_State;
				}
			}

			continue;
		}

		PushData(*It, pPacket);

		/// Check if data is incomplete
		if (m_State == State::STATE_INCOMPLETE)
			return m_State;
	}

	pBuffer->Pop(m_pDeserializePointer - &pBuffer->m_Data.front());

	return m_State;
}

void Serializer::PushData(InstructionType Type, Packet* pPacket)
{
	switch (Type)
	{
	case InstructionType::TYPE_INT8: pPacket->m_Data.push_back(DeserializeInt8()); break;
	case InstructionType::TYPE_INT16: pPacket->m_Data.push_back(DeserializeInt16()); break;
	case InstructionType::TYPE_INT32: pPacket->m_Data.push_back(DeserializeInt32()); break;
	case InstructionType::TYPE_INT64: pPacket->m_Data.push_back(DeserializeInt64()); break;
	case InstructionType::TYPE_UINT8: pPacket->m_Data.push_back(DeserializeUInt8()); break;
	case InstructionType::TYPE_UINT16: pPacket->m_Data.push_back(DeserializeUInt16()); break;
	case InstructionType::TYPE_UINT32: pPacket->m_Data.push_back(DeserializeUInt32()); break;
	case InstructionType::TYPE_UINT64: pPacket->m_Data.push_back(DeserializeUInt64()); break;
	case InstructionType::TYPE_BOOL: pPacket->m_Data.push_back((bool)DeserializeInt8()); break;
	case InstructionType::TYPE_DOUBLE: pPacket->m_Data.push_back(DeserializeDouble()); break;
	case InstructionType::TYPE_STRING: pPacket->m_Data.push_back(DeserializeString()); break;
	}
}

int8_t Serializer::DeserializeInt8()
{
	int8_t Value = DeserializeUInt8();
	return *reinterpret_cast<int8_t*>(&Value);
}

int16_t Serializer::DeserializeInt16()
{
	uint16_t Value = DeserializeUInt16();
	return *reinterpret_cast<uint16_t*>(&Value);
}

int32_t Serializer::DeserializeInt32()
{
	uint32_t Value = DeserializeUInt32();
	return *reinterpret_cast<int32_t*>(&Value);
}

int64_t Serializer::DeserializeInt64()
{
	uint64_t Value = DeserializeUInt64();
	return *reinterpret_cast<int64_t*>(&Value);
}

uint8_t Serializer::DeserializeUInt8()
{
	int TypeSize = sizeof(uint8_t);

	if (m_pDeserializePointer + TypeSize > m_pDeserializeEndPointer)
	{
		m_State = State::STATE_INCOMPLETE;
		return 0;
	}

	uint8_t Value = (uint8_t)m_pDeserializePointer[0];

	m_pDeserializePointer += TypeSize;

	return Value;
}

uint16_t Serializer::DeserializeUInt16()
{
	int TypeSize = sizeof(uint16_t);

	if (m_pDeserializePointer + TypeSize > m_pDeserializeEndPointer)
	{
		m_State = State::STATE_INCOMPLETE;
		return 0;
	}

	uint16_t Value = uint16_t(
		(uint16_t)((uint8_t)m_pDeserializePointer[0]) << 8 |
		(uint16_t)((uint8_t)m_pDeserializePointer[1])
	);

	m_pDeserializePointer += TypeSize;

	return Value;
}

uint32_t Serializer::DeserializeUInt32()
{
	int TypeSize = sizeof(uint32_t);

	if (m_pDeserializePointer + TypeSize > m_pDeserializeEndPointer)
	{
		m_State = State::STATE_INCOMPLETE;
		return 0;
	}

	uint32_t Value = uint32_t(
		(uint32_t)((uint8_t)m_pDeserializePointer[0]) << 24 |
		(uint32_t)((uint8_t)m_pDeserializePointer[1]) << 16 |
		(uint32_t)((uint8_t)m_pDeserializePointer[2]) << 8 |
		(uint32_t)((uint8_t)m_pDeserializePointer[3])
	);

	m_pDeserializePointer += TypeSize;

	return Value;
}

uint64_t Serializer::DeserializeUInt64()
{
	int TypeSize = sizeof(uint64_t);

	if (m_pDeserializePointer + TypeSize > m_pDeserializeEndPointer)
	{
		m_State = State::STATE_INCOMPLETE;
		return 0;
	}

	uint64_t Value = uint64_t(
		(uint64_t)((uint8_t)m_pDeserializePointer[0]) << 56 |
		(uint64_t)((uint8_t)m_pDeserializePointer[1]) << 48 |
		(uint64_t)((uint8_t)m_pDeserializePointer[2]) << 40 |
		(uint64_t)((uint8_t)m_pDeserializePointer[3]) << 32 |
		(uint64_t)((uint8_t)m_pDeserializePointer[4]) << 24 |
		(uint64_t)((uint8_t)m_pDeserializePointer[5]) << 16 |
		(uint64_t)((uint8_t)m_pDeserializePointer[6]) << 8 |
		(uint64_t)((uint8_t)m_pDeserializePointer[7])
	);

	m_pDeserializePointer += TypeSize;

	return Value;
}

double Serializer::DeserializeDouble()
{
	uint64_t Value = DeserializeUInt64();
	return *reinterpret_cast<double*>(&Value);
}

std::string Serializer::DeserializeString()
{
	int Length = DeserializeUInt32();
	int TypeSize = sizeof(char);

	if (m_pDeserializePointer + (TypeSize * Length) > m_pDeserializeEndPointer)
	{
		m_State = State::STATE_INCOMPLETE;
		return "";
	}

	std::string Value;
	Value.resize(Length);
	std::copy(m_pDeserializePointer, m_pDeserializePointer + Length, Value.data());

	m_pDeserializePointer += Length * sizeof(char);

	return Value;
}

void Serializer::SerializeInt8(PacketData Value)
{
	int8_t Data = std::get<int8_t>(Value);

	m_pSerializePointer[0] = (char)Data;
	m_pSerializePointer += sizeof(int8_t);
}

void Serializer::SerializeInt16(PacketData Value)
{
	int16_t Data = std::get<int16_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 8);
	m_pSerializePointer[1] = (uint8_t)Data;
	m_pSerializePointer += sizeof(int16_t);
}

void Serializer::SerializeInt32(PacketData Value)
{
	int32_t Data = std::get<int32_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 24);
	m_pSerializePointer[1] = (uint8_t)(Data >> 16);
	m_pSerializePointer[2] = (uint8_t)(Data >> 8);
	m_pSerializePointer[3] = (uint8_t)Data;
	m_pSerializePointer += sizeof(int32_t);
}

void Serializer::SerializeInt64(PacketData Value)
{
	int64_t Data = std::get<int64_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 56);
	m_pSerializePointer[1] = (uint8_t)(Data >> 48);
	m_pSerializePointer[2] = (uint8_t)(Data >> 40);
	m_pSerializePointer[3] = (uint8_t)(Data >> 32);
	m_pSerializePointer[4] = (uint8_t)(Data >> 24);
	m_pSerializePointer[5] = (uint8_t)(Data >> 16);
	m_pSerializePointer[6] = (uint8_t)(Data >> 8);
	m_pSerializePointer[7] = (uint8_t)Data;
	m_pSerializePointer += sizeof(int64_t);
}

void Serializer::SerializeUInt8(PacketData Value)
{
	uint8_t Data = std::get<uint8_t>(Value);

	m_pSerializePointer[0] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint8_t);
}

void Serializer::SerializeUInt16(PacketData Value)
{
	uint16_t Data = std::get<uint16_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 8);
	m_pSerializePointer[1] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint16_t);
}

void Serializer::SerializeUInt32(PacketData Value)
{
	uint32_t Data = std::get<uint32_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 24);
	m_pSerializePointer[1] = (uint8_t)(Data >> 16);
	m_pSerializePointer[2] = (uint8_t)(Data >> 8);
	m_pSerializePointer[3] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint32_t);
}

void Serializer::SerializeUInt64(PacketData Value)
{
	uint64_t Data = std::get<uint64_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 56);
	m_pSerializePointer[1] = (uint8_t)(Data >> 48);
	m_pSerializePointer[2] = (uint8_t)(Data >> 40);
	m_pSerializePointer[3] = (uint8_t)(Data >> 32);
	m_pSerializePointer[4] = (uint8_t)(Data >> 24);
	m_pSerializePointer[5] = (uint8_t)(Data >> 16);
	m_pSerializePointer[6] = (uint8_t)(Data >> 8);
	m_pSerializePointer[7] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint64_t);
}

void Serializer::SerializeDouble(PacketData Value)
{
	double get = std::get<double>(Value);
	uint64_t Data = *(uint64_t*)&get;

	m_pSerializePointer[0] = (uint8_t)(Data >> 56);
	m_pSerializePointer[1] = (uint8_t)(Data >> 48);
	m_pSerializePointer[2] = (uint8_t)(Data >> 40);
	m_pSerializePointer[3] = (uint8_t)(Data >> 32);
	m_pSerializePointer[4] = (uint8_t)(Data >> 24);
	m_pSerializePointer[5] = (uint8_t)(Data >> 16);
	m_pSerializePointer[6] = (uint8_t)(Data >> 8);
	m_pSerializePointer[7] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint64_t);
}

void Serializer::SerializeString(PacketData Value)
{
	std::string Data = std::get<std::string>(Value);

	uint32_t Length = (uint32_t)Data.length();
	SerializeUInt32(PacketData(Length));
	std::copy(Data.begin(), Data.end(), m_pSerializePointer);

	m_pSerializePointer += Length * sizeof(char);
}

void Serializer::SerializeBool(PacketData Value)
{
	bool Data = std::get<bool>(Value);

	m_pSerializePointer[0] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint8_t);
}
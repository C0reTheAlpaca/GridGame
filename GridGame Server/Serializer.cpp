#include "Serializer.h"
#include "DynamicBuffer.h"

Serializer::Serializer(const std::map<int, Instruction>* pInstructions)
{
	m_State = State::STATE_DEFAULT;
	m_pSerializeData = nullptr;
	m_pSerializePointer = nullptr;
	m_pDeserializePointer = nullptr;
	m_pDeserializeEndPointer = nullptr;
	m_pInstructions = pInstructions;
}

void Serializer::SerializeSend(int Magic, Packet Data, SOCKET Socket)
{
	std::size_t MagicSize = sizeof(Magic);

	auto InstructionIt = m_pInstructions->find(Magic);

	if (InstructionIt == m_pInstructions->end())
		return; // todo: Handle??

	Instruction Instruction = InstructionIt->second;

	// Calculate total bytes of packet
	std::size_t TotalPacketBytes = MagicSize;

	for (Variant Variant : Data)
	{
		std::size_t Index = Variant.index();
		TotalPacketBytes += m_DataSizes[Index];

		if ((InstructionType)Index == InstructionType::TYPE_STRING)
		{
			TotalPacketBytes += std::get<std::string>(Variant).length() * sizeof(char);
		}
	}

	m_pSerializeData = (char*)std::malloc(TotalPacketBytes);
	m_pSerializePointer = m_pSerializeData;
	
	// Serialize magic
	SerializeInt32(Variant(Magic));

	// Serialize data
	for (auto& Value : Data)
	{
		switch ((InstructionType)Value.index())
		{
		case InstructionType::TYPE_INT8:   SerializeInt8(Value); break;
		case InstructionType::TYPE_INT16:  SerializeInt16(Value); break;
		case InstructionType::TYPE_INT32:  SerializeInt32(Value); break;
		case InstructionType::TYPE_INT64:  SerializeInt64(Value); break;
		case InstructionType::TYPE_UINT8:  SerializeUInt8(Value); break;
		case InstructionType::TYPE_UINT16: SerializeUInt16(Value); break;
		case InstructionType::TYPE_UINT32: SerializeUInt32(Value); break;
		case InstructionType::TYPE_UINT64: SerializeUInt64(Value); break;
		case InstructionType::TYPE_BOOL:   SerializeUInt8(Value); break;
		case InstructionType::TYPE_DOUBLE: SerializeUInt64(Value); break;
		case InstructionType::TYPE_STRING: SerializeString(Value); break;
		default:
			return;
		}
	}

	// Send data
	send(Socket, m_pSerializeData, TotalPacketBytes, NULL);

	// Cleanup
	free(m_pSerializeData);
}

Serializer::State Serializer::Deserialize(DynamicBuffer* pBuffer, Data* pData)
{
	m_State = State::STATE_SUCCESS;

	// Check of buffer contains magic
	std::size_t MagicSize = sizeof(pData->Magic);

	if (pBuffer->GetSize() < sizeof(pData->Magic))
		return State::STATE_INCOMPLETE;

	m_pDeserializePointer = &pBuffer->m_Data.front();
	m_pDeserializeEndPointer = m_pDeserializePointer + pBuffer->GetSize();

	// Get magic
	int32_t Magic = DeserializeUInt32();

	auto InstructionIt = m_pInstructions->find(Magic);

	if (InstructionIt == m_pInstructions->end())
		return State::STATE_ERROR;

	Instruction Instruction = InstructionIt->second;

	pData->Magic = Magic;

	for (InstructionType Type : m_pInstructions->at(Magic))
	{
		switch (Type)
		{
		case InstructionType::TYPE_INT8: pData->Values.push_back(DeserializeInt8()); break;
		case InstructionType::TYPE_INT16: pData->Values.push_back(DeserializeInt16()); break;
		case InstructionType::TYPE_INT32: pData->Values.push_back(DeserializeInt32()); break;
		case InstructionType::TYPE_INT64: pData->Values.push_back(DeserializeInt64()); break;
		case InstructionType::TYPE_UINT8: pData->Values.push_back(DeserializeUInt8()); break;
		case InstructionType::TYPE_UINT16: pData->Values.push_back(DeserializeUInt16()); break;
		case InstructionType::TYPE_UINT32: pData->Values.push_back(DeserializeUInt32()); break;
		case InstructionType::TYPE_UINT64: pData->Values.push_back(DeserializeUInt64()); break;
		case InstructionType::TYPE_BOOL: pData->Values.push_back((bool)DeserializeInt8()); break;
		case InstructionType::TYPE_DOUBLE: pData->Values.push_back(DeserializeDouble()); break;
		case InstructionType::TYPE_STRING: pData->Values.push_back(DeserializeString()); break;
		}

		/// Check if data is incomplete
		if (m_State == State::STATE_INCOMPLETE)
			return m_State;
	}

	pBuffer->Pop(m_pDeserializePointer - &pBuffer->m_Data.front());

	return m_State;
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
		(uint64_t)((uint8_t)m_pDeserializePointer[6]) << 8  |
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

void Serializer::SerializeInt8(Variant Value)
{
	int8_t Data = std::get<int8_t>(Value);

	m_pSerializePointer[0] = (char)Data;
	m_pSerializePointer += sizeof(int8_t);
}

void Serializer::SerializeInt16(Variant Value)
{
	int16_t Data = std::get<int16_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 8);
	m_pSerializePointer[1] = (uint8_t)Data;
	m_pSerializePointer += sizeof(int16_t);
}

void Serializer::SerializeInt32(Variant Value)
{
	int32_t Data = std::get<int32_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 24);
	m_pSerializePointer[1] = (uint8_t)(Data >> 16);
	m_pSerializePointer[2] = (uint8_t)(Data >> 8);
	m_pSerializePointer[3] = (uint8_t)Data;
	m_pSerializePointer += sizeof(int32_t);
}

void Serializer::SerializeInt64(Variant Value)
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

void Serializer::SerializeUInt8(Variant Value)
{
	uint8_t Data = std::get<uint8_t>(Value);

	m_pSerializePointer[0] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint8_t);
}

void Serializer::SerializeUInt16(Variant Value)
{
	uint16_t Data = std::get<uint16_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 8);
	m_pSerializePointer[1] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint16_t);
}

void Serializer::SerializeUInt32(Variant Value)
{
	uint32_t Data = std::get<uint32_t>(Value);

	m_pSerializePointer[0] = (uint8_t)(Data >> 24);
	m_pSerializePointer[1] = (uint8_t)(Data >> 16);
	m_pSerializePointer[2] = (uint8_t)(Data >> 8);
	m_pSerializePointer[3] = (uint8_t)Data;
	m_pSerializePointer += sizeof(uint32_t);
}

void Serializer::SerializeUInt64(Variant Value)
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

void Serializer::SerializeDouble(Variant Value)
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

void Serializer::SerializeString(Variant Value)
{
	std::string Data = std::get<std::string>(Value);

	uint32_t Length = (uint32_t)Data.length();
	SerializeUInt32(Variant(Length));
	std::copy(Data.begin(), Data.end(), m_pSerializePointer);

	m_pSerializePointer += Length * sizeof(char);
}
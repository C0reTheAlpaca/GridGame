#include "DynamicBuffer.h"

DynamicBuffer::DynamicBuffer(std::size_t Bytes)
{
	m_DataBytes = 0;
	m_ReservedBytes = Bytes;
	m_Data.resize(Bytes);
}

void DynamicBuffer::Append(char* pData, std::size_t Bytes)
{
	if (m_DataBytes + Bytes > m_ReservedBytes)
	{
		m_ReservedBytes = m_DataBytes + Bytes;
		m_Data.resize(m_DataBytes + Bytes);
	}

	std::memcpy(&m_Data.data()[m_DataBytes], pData, Bytes);
	m_DataBytes += Bytes;
}

std::string DynamicBuffer::Read(std::size_t Bytes)
{
	std::string Output;
	Output.resize(Bytes);
	std::memcpy((void*)Output.data(), m_Data.data(), Bytes);

	return Output;
}

void DynamicBuffer::Pop(std::size_t Bytes)
{
	std::memcpy(m_Data.data(), &m_Data.data()[Bytes], m_DataBytes - Bytes);
	std::memset(&m_Data.data()[m_DataBytes - Bytes], 0, Bytes);

	m_DataBytes -= Bytes;
}

std::size_t DynamicBuffer::GetSize()
{
	return m_DataBytes;
}

void DynamicBuffer::Clear()
{
	m_DataBytes = 0;
	m_Data.clear();
}
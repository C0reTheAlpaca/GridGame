#pragma once
#include <vector>
#include <string>

class DynamicBuffer
{
public:
	DynamicBuffer(std::size_t Bytes);
	void Append(char* pData, std::size_t Bytes);
	void Pop(std::size_t Bytes);
	void Clear();
	std::string Read(std::size_t Bytes);
	std::size_t GetSize();

public:
	std::size_t m_DataBytes;
	std::size_t m_ReservedBytes;
	std::vector<char> m_Data;
};
#pragma once
#include <algorithm>
#include <ctype.h>
#include <random>

class Utility
{
public:
	template <typename T>
	static T GetRandomReal(T From, T To)
	{
		static std::random_device m_Device;
		static std::default_random_engine m_Engine(m_Device());
		std::uniform_real_distribution<T> Distribution(From, To);
		return Distribution(m_Engine);
	}

	template <typename T>
	static T GetRandomInteger(T From, T To)
	{
		static std::random_device m_Device;
		static std::default_random_engine m_Engine(m_Device());
		std::uniform_int_distribution<T> Distribution(From, To);
		return Distribution(m_Engine);
	}
};
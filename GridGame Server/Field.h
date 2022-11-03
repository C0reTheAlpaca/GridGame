#pragma once
#include "Player.h"

#define FIELD_NO_OWNER 255

class Field
{
public:
	enum class FieldType : int
	{
		FIELD_EMPTY,
		FIELD_FOOD,
		FIELD_WORKER,
	};

	Field();
	Field(FieldType Type, int OwnerID, int Power);
	void Reset();

	FieldType m_FieldType;
	uint8_t m_OwnerID;
	int16_t m_Power;
	bool m_WasMoved;
};

struct FieldUpdate
{
	uint32_t x;
	uint32_t y;
	Field Field;
};
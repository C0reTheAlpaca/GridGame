#pragma once
#include "Player.h"

#define FIELD_NO_OWNER 255

class Field
{
public:
	enum FieldType
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


struct Move
{
	uint32_t X;
	uint32_t Y;
	Field Field;
};
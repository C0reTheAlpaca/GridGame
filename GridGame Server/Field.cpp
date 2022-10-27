#include "Field.h"
#include "Player.h"

Field::Field()
{
	m_FieldType = FIELD_EMPTY;
	m_OwnerID = FIELD_NO_OWNER;
	m_Power = 0;
}

Field::Field(FieldType Type, int OwnerID, int Power)
{
	m_FieldType = Type;
	m_OwnerID = OwnerID;
	m_Power = Power;
}

void Field::Reset()
{
	*this = Field();
}
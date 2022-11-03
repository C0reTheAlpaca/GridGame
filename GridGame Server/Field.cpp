#include "Field.h"
#include "Player.h"

Field::Field()
{
	m_FieldType = FieldType::FIELD_EMPTY;
	m_OwnerID = FIELD_NO_OWNER;
	m_Power = 0;
	m_WasMoved = false;
}

Field::Field(FieldType Type, int OwnerID, int Power)
{
	m_FieldType = Type;
	m_OwnerID = OwnerID;
	m_Power = Power;
	m_WasMoved = false;
}

void Field::Reset()
{
	*this = Field();
}
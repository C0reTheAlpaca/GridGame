#pragma once
#include "Instruction.h"

Instruction Connect = {
		InstructionType::TYPE_STRING,         // Name
};

Instruction ConnectAck = {
	InstructionType::TYPE_UINT8,          // Player ID
};

Instruction GameStart = {
	InstructionType::TYPE_UINT32,         // Grid width
	InstructionType::TYPE_UINT32,         // Grid height
	InstructionStructure {                // Players[]
		{
			InstructionType::TYPE_UINT8,  // PlayerID
			InstructionType::TYPE_STRING,  // PlayerID
		}
	},
};

Instruction Move = {
	InstructionType::TYPE_BOOL,           // Should split
	InstructionType::TYPE_UINT32,         // From X
	InstructionType::TYPE_UINT32,         // From Y
	InstructionType::TYPE_UINT32,         // To X
	InstructionType::TYPE_UINT32,         // To Y
};

Instruction Broadcast = {
	InstructionType::TYPE_STRING,         // Message
};

Instruction GameData = {
	InstructionType::TYPE_UINT8,          // Turn player ID
	InstructionType::TYPE_INT64,          // Time epoch move timeout
	InstructionStructure {                // Updated fields[]
		{
			InstructionType::TYPE_UINT8,  // X
			InstructionType::TYPE_UINT8,  // Y
			InstructionType::TYPE_UINT8,  // Type ID
			InstructionType::TYPE_UINT8,  // Owner ID
			InstructionType::TYPE_INT16   // Power
		}
	},
	InstructionStructure {                // Next food spawns[]
		{
			InstructionType::TYPE_UINT8,  // X
			InstructionType::TYPE_UINT8,  // Y
		}
	}
};
#pragma once
#include <vector>

enum class InstructionType : int
{
    TYPE_INT8,
    TYPE_INT16,
    TYPE_INT32,
    TYPE_INT64,
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT32,
    TYPE_UINT64,
    TYPE_BOOL,
    TYPE_DOUBLE,
    TYPE_STRING
};

typedef std::vector<InstructionType> Instruction;
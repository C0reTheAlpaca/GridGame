#pragma once
#include <map>
#include <vector>
#include <variant>
#include <initializer_list>

#define INSTRUCTION_TYPE 0
#define INSTRUCTION_STRUCTURE 1

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

class InstructionStructure
{
public:
    InstructionStructure(std::initializer_list<InstructionType> Types)
    {
        for (InstructionType Type : Types)
        {
            m_Types.push_back(Type);
        }
    }

    std::vector<InstructionType> m_Types;
};

typedef std::variant<InstructionType, InstructionStructure> InstructionVariant;

class Instruction
{
public:

    Instruction()
    {

    }

    Instruction(std::initializer_list<InstructionVariant> Variant)
    {
        for (InstructionVariant Variant : Variant)
        {
            switch (Variant.index())
            {
            case INSTRUCTION_TYPE:
                m_Types.push_back(std::get<InstructionType>(Variant));
                break;
            case INSTRUCTION_STRUCTURE:
                InstructionStructure Structure = std::get<InstructionStructure>(Variant);
                m_Types.push_back(InstructionType::TYPE_UINT32);
                m_StructLookup[(int)std::distance(m_Types.begin(), m_Types.end())] = Structure.m_Types;

                for (InstructionType Type : Structure.m_Types)
                {
                    m_Types.push_back(Type);
                }
                break;
            }
        }
    }

    std::vector<InstructionType> m_Types;
    std::map<int, std::vector<InstructionType>> m_StructLookup;
};
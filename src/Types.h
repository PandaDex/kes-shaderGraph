#pragma once

#include <QString>

namespace sne {

enum class PortType { Float, Float2, Float3, Float4, Int, Bool, Generic };

inline int dimension(PortType t)
{
    switch (t) {
    case PortType::Float2: return 2;
    case PortType::Float3: return 3;
    case PortType::Float4: return 4;
    case PortType::Generic: return 0;
    default: return 1;
    }
}

inline bool isFloatFamily(PortType t)
{
    return t == PortType::Float || t == PortType::Float2 || t == PortType::Float3 || t == PortType::Float4;
}

inline PortType floatTypeOfDim(int d)
{
    switch (d) {
    case 2: return PortType::Float2;
    case 3: return PortType::Float3;
    case 4: return PortType::Float4;
    default: return PortType::Float;
    }
}

inline QString slangTypeName(PortType t)
{
    switch (t) {
    case PortType::Float2: return QStringLiteral("float2");
    case PortType::Float3: return QStringLiteral("float3");
    case PortType::Float4: return QStringLiteral("float4");
    case PortType::Int: return QStringLiteral("int");
    case PortType::Bool: return QStringLiteral("bool");
    default: return QStringLiteral("float");
    }
}

} // namespace sne

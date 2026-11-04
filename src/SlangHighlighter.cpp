#include "SlangHighlighter.h"

namespace sne {

SlangHighlighter::SlangHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc)
{
    auto fmt = [](const char* color, bool bold = false, bool italic = false) {
        QTextCharFormat f;
        f.setForeground(QColor(color));
        if (bold) f.setFontWeight(QFont::Bold);
        f.setFontItalic(italic);
        return f;
    };
    auto words = [](const QStringList& l) { return QRegularExpression("\\b(" + l.join('|') + ")\\b"); };

    m_rules.append({QRegularExpression(R"(\b[0-9]+\.?[0-9]*([eE][+-]?[0-9]+)?f?\b)"), fmt("#d19a66")});
    m_rules.append({words({"struct", "return", "if", "else", "for", "while", "do", "break", "continue", "const",
                           "in", "out", "inout", "uniform", "static", "import", "typedef", "true", "false",
                           "interface", "extension", "namespace", "using"}),
                    fmt("#c678dd", true)});
    m_rules.append({words({"void", "bool", "int", "uint", "float", "half", "double", "float2", "float3", "float4",
                           "int2", "int3", "int4", "uint2", "uint3", "uint4", "float2x2", "float3x3", "float4x4",
                           "Texture2D", "SamplerState", "ConstantBuffer", "StructuredBuffer", "RWTexture2D"}),
                    fmt("#e5c07b")});
    m_rules.append({words({"sin", "cos", "tan", "atan2", "pow", "sqrt", "abs", "min", "max", "clamp", "saturate",
                           "lerp", "step", "smoothstep", "frac", "floor", "ceil", "fmod", "dot", "cross", "length",
                           "distance", "normalize", "reflect", "mul", "float", "int", "exp", "log", "sign"}),
                    fmt("#61afef")});
    m_rules.append({QRegularExpression(R"(\b(sn_\w+))"), fmt("#56b6c2")});
    m_rules.append({QRegularExpression(R"(\b(n\d+_\w+)\b)"), fmt("#98c379")});
    m_rules.append({QRegularExpression(R"(\b(SV_\w+|POSITION|NORMAL|TEXCOORD\d*)\b)"), fmt("#e06c75")});
    m_rules.append({QRegularExpression(R"(\[\s*shader\s*\(\s*"[^"]*"\s*\)\s*\])"), fmt("#e06c75", true)});
    m_rules.append({QRegularExpression("\"[^\"]*\""), fmt("#98c379")});
    m_commentFmt = fmt("#7f848e", false, true);
}

void SlangHighlighter::highlightBlock(const QString& text)
{
    for (const Rule& r : std::as_const(m_rules)) {
        auto it = r.re.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(int(m.capturedStart()), int(m.capturedLength()), r.fmt);
        }
    }
    const int c = text.indexOf("//");
    if (c >= 0) setFormat(c, text.length() - c, m_commentFmt);
}

} // namespace sne

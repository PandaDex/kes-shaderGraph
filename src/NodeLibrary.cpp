#include "NodeLibrary.h"

#include "MaterialSettings.h"

#include <initializer_list>

namespace sne {

namespace {
QVariantList V(std::initializer_list<double> l)
{
    QVariantList r;
    for (double d : l) r << d;
    return r;
}
} // namespace

const NodeLibrary& NodeLibrary::instance()
{
    static NodeLibrary lib;
    return lib;
}

const NodeDef* NodeLibrary::find(const QString& type) const
{
    auto it = m_index.constFind(type);
    return it == m_index.constEnd() ? nullptr : &m_defs[it.value()];
}

const HelperDef* NodeLibrary::helper(const QString& key) const
{
    for (const HelperDef& h : m_helpers) if (h.key == key) return &h;
    return nullptr;
}

QStringList NodeLibrary::categories() const
{
    static const QStringList order = {"Input", "Constant", "Property", "Math", "Vector", "UV",
                                      "Texture", "Procedural", "Color", "Custom", "Output"};
    QStringList result;
    for (const QString& c : order) {
        for (const NodeDef& d : m_defs) {
            if (d.category == c) { result << c; break; }
        }
    }
    for (const NodeDef& d : m_defs) if (!result.contains(d.category)) result << d.category;
    return result;
}

NodeLibrary::NodeLibrary()
{
    const PortType G = PortType::Generic;
    const PortType F = PortType::Float;
    const PortType F2 = PortType::Float2;
    const PortType F3 = PortType::Float3;
    const PortType F4 = PortType::Float4;
    const PortType I = PortType::Int;

    auto add = [&](const char* type, const char* title, const char* cat, const char* desc) -> NodeDef& {
        NodeDef d;
        d.type = type;
        d.title = title;
        d.category = cat;
        d.description = desc;
        m_defs.append(d);
        return m_defs.last();
    };

    add("uv", "UV", "Input", "Mesh texture coordinates (TEXCOORD0).")
        .out("UV", F2, "input.uv");
    add("time", "Time", "Input", "Seconds since start, from the scene constant buffer.")
        .out("Time", F, "gScene.time")
        .out("Sin", F, "sin(gScene.time)")
        .out("Cos", F, "cos(gScene.time)");
    add("worldPosition", "World Position", "Input", "Interpolated world-space position.")
        .out("Position", F3, "input.worldPos");
    add("worldNormal", "World Normal", "Input", "Interpolated, normalised world-space normal.")
        .out("Normal", F3, "normalize(input.normal)");
    add("viewDirection", "View Direction", "Input", "Normalised vector from the surface towards the camera.")
        .out("Direction", F3, "normalize(gScene.cameraPos - input.worldPos)");
    add("material", "Material", "Input", "Values from the Material panel, after its textures. KES target only.")
        .out("Base Color", F3, "surface.baseColor")
        .out("Alpha", F, "surface.alpha")
        .out("Emissive", F3, "surface.emissive")
        .out("Metallic", F, "surface.metallic")
        .out("Roughness", F, "surface.roughness")
        .out("Occlusion", F, "surface.occlusion")
        .forEngineOnly();

    add("float", "Float", "Constant", "A constant scalar.")
        .pFloat("Value", 1.0)
        .out("Out", F, "${Value}");
    add("vector2", "Vector 2", "Constant", "A constant float2.")
        .pFloat("X", 0.0).pFloat("Y", 0.0)
        .out("Out", F2, "float2(${X}, ${Y})");
    add("vector3", "Vector 3", "Constant", "A constant float3.")
        .pFloat("X", 0.0).pFloat("Y", 0.0).pFloat("Z", 0.0)
        .out("Out", F3, "float3(${X}, ${Y}, ${Z})");
    add("vector4", "Vector 4", "Constant", "A constant float4.")
        .pFloat("X", 0.0).pFloat("Y", 0.0).pFloat("Z", 0.0).pFloat("W", 0.0)
        .out("Out", F4, "float4(${X}, ${Y}, ${Z}, ${W})");
    add("color", "Color", "Constant", "A constant RGBA colour.")
        .pColor("Color", 0.8, 0.8, 0.8, 1.0)
        .out("RGBA", F4, "float4(${Color})")
        .out("RGB", F3, "float4(${Color}).rgb")
        .out("Alpha", F, "float4(${Color}).a");

    add("propFloat", "Float Property", "Property", "A float in the material constant buffer, set from the application.")
        .pIdent("Name", "intensity")
        .pFloat("Default", 1.0)
        .uniform("Name", F)
        .out("Out", F, "gMaterial.${Name}");
    add("propVector3", "Vector 3 Property", "Property", "A float3 in the material constant buffer.")
        .pIdent("Name", "direction")
        .pFloat("X", 0.0).pFloat("Y", 1.0).pFloat("Z", 0.0)
        .uniform("Name", F3)
        .out("Out", F3, "gMaterial.${Name}");
    add("propColor", "Color Property", "Property", "A float4 colour in the material constant buffer.")
        .pIdent("Name", "tint")
        .pColor("Default", 1.0, 1.0, 1.0, 1.0)
        .uniform("Name", F4)
        .out("RGBA", F4, "gMaterial.${Name}")
        .out("RGB", F3, "gMaterial.${Name}.rgb")
        .out("Alpha", F, "gMaterial.${Name}.a");

    auto binary = [&](const char* type, const char* title, const char* desc, const char* expr, double a, double b) {
        add(type, title, "Math", desc).in("A", G, V({a})).in("B", G, V({b})).out("Out", G, expr);
    };
    auto unary = [&](const char* type, const char* title, const char* desc, const char* expr) {
        add(type, title, "Math", desc).in("A", G, V({0.0})).out("Out", G, expr);
    };
    binary("add", "Add", "A + B", "{A} + {B}", 0, 0);
    binary("subtract", "Subtract", "A - B", "{A} - {B}", 0, 0);
    binary("multiply", "Multiply", "A * B (component-wise)", "{A} * {B}", 1, 1);
    binary("divide", "Divide", "A / B (component-wise)", "{A} / {B}", 1, 1);
    binary("power", "Power", "pow(A, B)", "pow({A}, {B})", 2, 2);
    binary("min", "Minimum", "min(A, B)", "min({A}, {B})", 0, 0);
    binary("max", "Maximum", "max(A, B)", "max({A}, {B})", 0, 0);
    binary("modulo", "Modulo", "fmod(A, B)", "fmod({A}, {B})", 1, 1);
    unary("abs", "Absolute", "abs(A)", "abs({A})");
    unary("negate", "Negate", "-A", "-{A}");
    unary("saturate", "Saturate", "Clamp to 0..1", "saturate({A})");
    unary("sqrt", "Square Root", "sqrt(A)", "sqrt({A})");
    unary("floor", "Floor", "floor(A)", "floor({A})");
    unary("ceil", "Ceiling", "ceil(A)", "ceil({A})");
    unary("fract", "Fraction", "frac(A)", "frac({A})");
    unary("sin", "Sine", "sin(A)", "sin({A})");
    unary("cos", "Cosine", "cos(A)", "cos({A})");
    unary("oneMinus", "One Minus", "1 - A", "(1.0 - {A})");

    add("lerp", "Lerp", "Math", "Linear interpolation: lerp(A, B, T).")
        .in("A", G, V({0.0})).in("B", G, V({1.0})).in("T", G, V({0.5}))
        .out("Out", G, "lerp({A}, {B}, {T})");
    add("clamp", "Clamp", "Math", "clamp(Value, Min, Max)")
        .in("Value", G, V({0.0})).in("Min", G, V({0.0})).in("Max", G, V({1.0}))
        .out("Out", G, "clamp({Value}, {Min}, {Max})");
    add("smoothstep", "Smoothstep", "Math", "Hermite interpolation between two edges.")
        .in("Edge0", G, V({0.0})).in("Edge1", G, V({1.0})).in("X", G, V({0.5}))
        .out("Out", G, "smoothstep({Edge0}, {Edge1}, {X})");
    add("step", "Step", "Math", "0 where X < Edge, else 1.")
        .in("Edge", G, V({0.5})).in("X", G, V({0.0}))
        .out("Out", G, "step({Edge}, {X})");
    add("remap", "Remap", "Math", "Remap Value from [InMin, InMax] to [OutMin, OutMax].")
        .in("Value", G, V({0.0})).in("InMin", G, V({0.0})).in("InMax", G, V({1.0}))
        .in("OutMin", G, V({0.0})).in("OutMax", G, V({1.0}))
        .out("Out", G, "({OutMin} + ({Value} - {InMin}) * ({OutMax} - {OutMin}) / ({InMax} - {InMin}))");
    add("dot", "Dot Product", "Math", "dot(A, B)")
        .in("A", G, V({0.0})).in("B", G, V({0.0})).minDim(2)
        .out("Out", F, "dot({A}, {B})");
    add("length", "Length", "Math", "Vector length.")
        .in("A", G, V({0.0})).minDim(2)
        .out("Out", F, "length({A})");
    add("distance", "Distance", "Math", "Distance between two points.")
        .in("A", G, V({0.0})).in("B", G, V({0.0})).minDim(2)
        .out("Out", F, "distance({A}, {B})");
    add("normalize", "Normalize", "Math", "Unit-length vector.")
        .in("A", G, V({1.0})).minDim(2)
        .out("Out", G, "normalize({A})");
    add("reflect", "Reflect", "Math", "reflect(Incident, Normal)")
        .in("Incident", G, V({0.0})).in("Normal", G, V({1.0})).minDim(2)
        .out("Out", G, "reflect({Incident}, {Normal})");
    add("cross", "Cross Product", "Math", "cross(A, B)")
        .in("A", F3, V({0, 0, 1})).in("B", F3, V({1, 0, 0}))
        .out("Out", F3, "cross({A}, {B})");

    add("split", "Split", "Vector", "Break a vector into components.")
        .in("In", F4, V({0, 0, 0, 0}))
        .out("R", F, "{In}.x").out("G", F, "{In}.y").out("B", F, "{In}.z").out("A", F, "{In}.w");
    add("combine", "Combine", "Vector", "Build vectors from scalar components.")
        .in("R", F, V({0.0})).in("G", F, V({0.0})).in("B", F, V({0.0})).in("A", F, V({1.0}))
        .out("RGBA", F4, "float4({R}, {G}, {B}, {A})")
        .out("RGB", F3, "float3({R}, {G}, {B})")
        .out("RG", F2, "float2({R}, {G})");

    add("tilingOffset", "Tiling And Offset", "UV", "UV * Tiling + Offset")
        .in("UV", F2, {}, "input.uv").in("Tiling", F2, V({1, 1})).in("Offset", F2, V({0, 0}))
        .out("Out", F2, "{UV} * {Tiling} + {Offset}");
    add("rotateUV", "Rotate UV", "UV", "Rotate UV around a centre point (radians).")
        .in("UV", F2, {}, "input.uv").in("Center", F2, V({0.5, 0.5})).in("Angle", F, V({0.0}))
        .out("Out", F2, "(mul(float2x2(cos({Angle}), -sin({Angle}), sin({Angle}), cos({Angle})), {UV} - {Center}) + {Center})");
    add("polarCoordinates", "Polar Coordinates", "UV", "Radius and angle of UV relative to Center.")
        .in("UV", F2, {}, "input.uv").in("Center", F2, V({0.5, 0.5}))
        .out("Radius", F, "length({UV} - {Center})")
        .out("Angle", F, "atan2(({UV} - {Center}).y, ({UV} - {Center}).x)");

    add("sampleTexture2D", "Sample Texture 2D", "Texture",
        "Sample a texture. The KES target reads the Material panel texture in Slot; "
        "standalone Slang declares a Texture2D named by Texture.")
        .pIdent("Texture", "albedo")
        .pEnum("Slot", materialSlotLabels(), materialSlotLabels().first())
        .texture("Texture")
        .in("UV", F2, {}, "input.uv")
        .out("RGBA", F4, "tex_${Texture}.Sample(gSampler, {UV})")
        .out("RGB", F3, "tex_${Texture}.Sample(gSampler, {UV}).rgb")
        .out("A", F, "tex_${Texture}.Sample(gSampler, {UV}).a");

    add("unpackNormal", "Unpack Normal", "Texture", "Decode a normal map sample to a tangent-space normal for the output's Normal input.")
        .in("RGB", F3, V({0.5, 0.5, 1.0})).in("Strength", F, V({1.0}))
        .out("Normal", F3, "normalize(float3(({RGB}.xy * 2.0 - 1.0) * {Strength}, {RGB}.z * 2.0 - 1.0))");

    add("checker", "Checkerboard", "Procedural", "0/1 checker pattern.")
        .in("UV", F2, {}, "input.uv").in("Scale", F, V({8.0}))
        .needs({"checker"})
        .out("Out", F, "sn_checker({UV}, {Scale})");
    add("valueNoise", "Value Noise", "Procedural", "Smooth 2D value noise in 0..1.")
        .in("UV", F2, {}, "input.uv").in("Scale", F, V({5.0}))
        .needs({"valueNoise"})
        .out("Out", F, "sn_valueNoise({UV} * {Scale})");
    add("fbm", "Fractal Noise", "Procedural", "Fractal Brownian motion built from value noise.")
        .in("UV", F2, {}, "input.uv").in("Scale", F, V({4.0})).in("Octaves", I, V({4}))
        .needs({"fbm"})
        .out("Out", F, "sn_fbm({UV} * {Scale}, {Octaves})");
    add("voronoi", "Voronoi", "Procedural", "Cellular noise: distance to nearest feature point and a per-cell random id.")
        .in("UV", F2, {}, "input.uv").in("Scale", F, V({5.0}))
        .needs({"voronoi"})
        .out("Distance", F, "sn_voronoi({UV} * {Scale}).x")
        .out("Cell", F, "sn_voronoi({UV} * {Scale}).y");
    add("fresnel", "Fresnel", "Procedural", "Rim term: pow(1 - saturate(N.V), Power).")
        .in("Power", F, V({5.0}))
        .out("Out", F, "pow(1.0 - saturate(dot(normalize(input.normal), normalize(gScene.cameraPos - input.worldPos))), {Power})");

    add("luminance", "Luminance", "Color", "Rec.709 luminance of an RGB colour.")
        .in("RGB", F3, V({0, 0, 0}))
        .out("Out", F, "dot({RGB}, float3(0.2126, 0.7152, 0.0722))");

    add("customExpression", "Custom Expression", "Custom",
        "Write a Slang expression using A, B, C, D. Inputs keep the type of whatever is connected; "
        "choose the output type to match your expression.")
        .in("A", G, V({0.0})).in("B", G, V({0.0})).in("C", G, V({0.0})).in("D", G, V({0.0}))
        .pEnum("Output", {"float", "float2", "float3", "float4"}, "float")
        .pCode("Expression", "A + B")
        .out("Out", F, "")
        .withRole(NodeRole::CustomExpression);

    add("fragmentOutput", "Fragment Output", "Output",
        "KES target: the surface fed to the engine's lighting; inputs left alone keep the Material panel values. "
        "Standalone Slang: float4(Color + Emission, Alpha), other inputs unused.")
        .in("Color", F3, V({0.8, 0.8, 0.8})).in("Emission", F3, V({0, 0, 0})).in("Alpha", F, V({1.0}))
        .in("Metallic", F, {}, "surface.metallic").in("Roughness", F, {}, "surface.roughness")
        .in("Normal", F3, {}, "surface.normal").in("Occlusion", F, {}, "surface.occlusion")
        .withRole(NodeRole::FragmentOutput);

    for (int i = 0; i < m_defs.size(); ++i) m_index.insert(m_defs[i].type, i);

    m_helpers = {
        {"hash21", {}, R"(float sn_hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    float d = dot(p, p + float2(45.32, 45.32));
    p += float2(d, d);
    return frac(p.x * p.y);
})"},
        {"hash22", {}, R"(float2 sn_hash22(float2 p)
{
    float3 p3 = frac(float3(p.x, p.y, p.x) * float3(0.1031, 0.1030, 0.0973));
    float d = dot(p3, p3.yzx + float3(33.33, 33.33, 33.33));
    p3 += float3(d, d, d);
    return frac((p3.xx + p3.yz) * p3.zy);
})"},
        {"valueNoise", {"hash21"}, R"(float sn_valueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = sn_hash21(i);
    float b = sn_hash21(i + float2(1.0, 0.0));
    float c = sn_hash21(i + float2(0.0, 1.0));
    float d = sn_hash21(i + float2(1.0, 1.0));
    float2 u = f * f * (float2(3.0, 3.0) - 2.0 * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
})"},
        {"fbm", {"valueNoise"}, R"(float sn_fbm(float2 p, int octaves)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; i++)
    {
        value += amplitude * sn_valueNoise(p);
        p = p * 2.0 + float2(17.0, 17.0);
        amplitude *= 0.5;
    }
    return value;
})"},
        {"voronoi", {"hash21", "hash22"}, R"(float2 sn_voronoi(float2 p)
{
    float2 n = floor(p);
    float2 f = frac(p);
    float minDist = 8.0;
    float cell = 0.0;
    for (int j = -1; j <= 1; j++)
    {
        for (int i = -1; i <= 1; i++)
        {
            float2 g = float2(float(i), float(j));
            float2 o = sn_hash22(n + g);
            float2 r = g + o - f;
            float d = dot(r, r);
            if (d < minDist)
            {
                minDist = d;
                cell = sn_hash21(n + g);
            }
        }
    }
    return float2(sqrt(minDist), cell);
})"},
        {"checker", {}, R"(float sn_checker(float2 uv, float scale)
{
    float2 c = floor(uv * scale);
    return frac((c.x + c.y) * 0.5) * 2.0;
})"},
    };
}

} // namespace sne

#include "ExampleGraphs.h"
#include "GraphModel.h"
#include "KesProject.h"
#include "MaterialExport.h"
#include "SlangCodeGen.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>

using namespace sne;

static int g_failures = 0;
#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;            \
            ++g_failures;                                                                \
        }                                                                                \
    } while (0)

static int connectByName(GraphModel& m, int from, const char* fp, int to, const char* tp)
{
    return m.addConnection(from, m.def(from)->outputIndex(fp), to, m.def(to)->inputIndex(tp));
}

static void testConversions()
{
    CHECK(SlangCodeGen::convertExpr("x", PortType::Float, PortType::Float3) == "float3(x)");
    CHECK(SlangCodeGen::convertExpr("x", PortType::Float4, PortType::Float3) == "x.xyz");
    CHECK(SlangCodeGen::convertExpr("x", PortType::Float3, PortType::Float) == "x.x");
    CHECK(SlangCodeGen::convertExpr("x", PortType::Float3, PortType::Float4) == "float4(x, 1.0)");
    CHECK(SlangCodeGen::convertExpr("x", PortType::Float2, PortType::Float3) == "float3(x, 0.0)");
    CHECK(SlangCodeGen::convertExpr("x", PortType::Int, PortType::Float) == "float(x)");
    CHECK(SlangCodeGen::formatFloat(1.0) == "1.0");
    CHECK(SlangCodeGen::formatFloat(0.25) == "0.25");
    CHECK(SlangCodeGen::sanitizeIdentifier("my tex!") == "my_tex_");
    CHECK(SlangCodeGen::sanitizeIdentifier("1abc") == "_1abc");
}

static void testModelAndUndo()
{
    GraphModel m;
    int a = -1, b = -1;
    m.edit("add", [&] {
        a = m.addNode("uv", {0, 0});
        b = m.addNode("fragmentOutput", {100, 0});
    });
    CHECK(m.nodeCount() == 2);

    int dup = 0;
    m.edit("dup", [&] { dup = m.addNode("fragmentOutput", {0, 0}); });
    CHECK(dup == -1);

    int c = -1;
    m.edit("connect", [&] { c = connectByName(m, a, "UV", b, "Color"); });
    CHECK(c > 0);
    CHECK(m.connections().size() == 1);

    m.undoStack()->undo();
    CHECK(m.connections().isEmpty());
    CHECK(m.nodeCount() == 2);
    m.undoStack()->redo();
    CHECK(m.connections().size() == 1);
    m.undoStack()->undo();
    m.undoStack()->undo();
    CHECK(m.nodeCount() == 0);

    m.undoStack()->clear();
    int n = -1;
    m.edit("add", [&] { n = m.addNode("float", {0, 0}); });
    for (double v : {1.0, 2.0, 3.0})
        m.edit("value", [&] { m.setParam(n, "Value", v); }, "param:float");
    CHECK(m.undoStack()->count() == 2);
    m.undoStack()->undo();
    CHECK(m.param(n, "Value").toDouble() == 1.0);
}

static void testCycleRejection()
{
    GraphModel m;
    int a, b, c;
    m.edit("setup", [&] {
        a = m.addNode("add", {0, 0});
        b = m.addNode("add", {1, 0});
        c = m.addNode("add", {2, 0});
        connectByName(m, a, "Out", b, "A");
        connectByName(m, b, "Out", c, "A");
    });
    QString why;
    CHECK(!m.canConnect(c, 0, a, 0, &why));
    CHECK(why.contains("cycle"));
    CHECK(!m.canConnect(a, 0, a, 1));
}

static void testTypeInference()
{
    GraphModel m;
    int uv = m.addNode("uv", {});
    int f = m.addNode("float", {});
    int mul = m.addNode("multiply", {});
    connectByName(m, f, "Out", mul, "A");
    TypeInfo t1 = inferTypes(m);
    CHECK(t1.outputType(mul, 0) == PortType::Float);

    connectByName(m, uv, "UV", mul, "B");
    TypeInfo t2 = inferTypes(m);
    CHECK(t2.outputType(mul, 0) == PortType::Float2);

    int dot = m.addNode("dot", {});
    CHECK(inferTypes(m).genericType(dot) == PortType::Float2);
}

static void testJsonRoundTrip()
{
    GraphModel m;
    buildExampleGraph(m);
    const QJsonObject j1 = m.toJson();
    GraphModel m2;
    QStringList warn;
    CHECK(m2.loadJson(j1, &warn));
    CHECK(warn.isEmpty());
    CHECK(m2.toJson() == j1);
    CHECK(m2.nodeCount() == m.nodeCount());
    CHECK(m2.connections().size() == m.connections().size());
}

static void testPasteRemapsIds()
{
    GraphModel m;
    buildExampleGraph(m);
    const int before = m.nodeCount();
    const QList<int> ids = m.nodeIds();
    QJsonObject clip = m.toJson(&ids);
    QList<int> pasted;
    m.edit("paste", [&] { pasted = m.insertJson(clip, QPointF(30, 30)); });
    CHECK(m.nodeCount() == before * 2 - 1);
    CHECK(pasted.size() == before - 1);
}

static void testCodegen(const QString& dumpPath)
{
    GraphModel m;
    buildExampleGraph(m);
    CodeGenResult r = SlangCodeGen::generate(m);
    CHECK(r.ok);
    CHECK(r.errors.isEmpty());
    CHECK(r.code.contains("[shader(\"fragment\")]"));
    CHECK(r.code.contains("[shader(\"vertex\")]"));
    CHECK(r.code.contains("float sn_fbm(float2 p, int octaves)"));
    CHECK(r.code.contains("float sn_valueNoise(float2 p)"));
    CHECK(r.code.contains("float sn_hash21(float2 p)"));
    CHECK(!r.code.contains("sn_voronoi"));
    CHECK(r.code.contains("return float4("));
    CHECK(r.code.indexOf("sn_hash21(float2 p)") < r.code.indexOf("sn_valueNoise(float2 p)"));
    CHECK(r.code.indexOf("sn_valueNoise(float2 p)") < r.code.indexOf("sn_fbm(float2 p"));
    CHECK(r.code.contains("n2_Time * 0.08"));
    CHECK(!r.code.contains("n2_Sin"));
    CHECK(!r.code.contains("n7_RGBA"));
    CHECK(r.code.contains("float3 n9_Out = lerp("));
    int mapped = 0;
    for (auto it = r.lineToNode.cbegin(); it != r.lineToNode.cend(); ++it) {
        const QString line = r.code.split('\n').value(it.key() - 1);
        if (line.contains("n9_Out =") && it.value() == 9) ++mapped;
    }
    CHECK(mapped == 1);

    if (!dumpPath.isEmpty()) {
        QFile f(dumpPath);
        if (f.open(QIODevice::WriteOnly)) f.write(r.code.toUtf8());
        QFile j(dumpPath + ".slangnodes");
        if (j.open(QIODevice::WriteOnly)) j.write(QJsonDocument(m.toJson()).toJson(QJsonDocument::Indented));
    }
}

static void testPropertiesTexturesCustom()
{
    GraphModel m;
    int out = m.addNode("fragmentOutput", {});
    int tex = m.addNode("sampleTexture2D", {});
    m.setParam(tex, "Texture", "my albedo");
    int prop = m.addNode("propFloat", {});
    m.setParam(prop, "Name", "glow");
    int prop2 = m.addNode("propFloat", {});
    m.setParam(prop2, "Name", "glow");
    int mul = m.addNode("multiply", {});
    int custom = m.addNode("customExpression", {});
    m.setParam(custom, "Output", "float3");
    m.setParam(custom, "Expression", "A * B + float3(C, C, C)");
    connectByName(m, tex, "RGB", custom, "A");
    connectByName(m, prop, "Out", custom, "B");
    connectByName(m, prop2, "Out", mul, "A");
    connectByName(m, mul, "Out", custom, "C");
    connectByName(m, custom, "Out", out, "Color");

    CodeGenResult r = SlangCodeGen::generate(m);
    CHECK(r.ok);
    CHECK(r.code.contains("Texture2D tex_my_albedo;"));
    CHECK(r.code.contains("struct MaterialParams"));
    CHECK(r.code.count("    float glow;") == 1);
    CHECK(r.code.contains("float3 A = n"));
    CHECK(r.code.contains("float B = n"));
    CHECK(r.code.contains("= A * B + float3(C, C, C);"));
    CHECK(r.code.contains("tex_my_albedo.Sample(gSampler, input.uv).rgb"));

    int vec = m.addNode("propVector3", {});
    m.setParam(vec, "Name", "glow");
    connectByName(m, vec, "Out", mul, "B");
    CHECK(!SlangCodeGen::generate(m).ok);
}

static void testEngineTarget()
{
    GraphModel m;
    const int out = m.addNode("fragmentOutput", {});
    const int tex = m.addNode("sampleTexture2D", {});
    m.setParam(tex, "Slot", "Emissive");
    const int time = m.addNode("time", {});
    const int fresnel = m.addNode("fresnel", {});
    const int prop = m.addNode("propFloat", {});
    m.setParam(prop, "Name", "glow");
    m.setParam(prop, "Default", 2.5);
    const int mat = m.addNode("material", {});
    connectByName(m, tex, "RGB", out, "Emission");
    connectByName(m, time, "Sin", out, "Metallic");
    connectByName(m, fresnel, "Out", out, "Roughness");
    connectByName(m, prop, "Out", out, "Occlusion");
    connectByName(m, mat, "Base Color", out, "Normal");

    CodeGenResult r = SlangCodeGen::generate(m, "t", ShaderTarget::KesEngine);
    CHECK(r.ok);
    CHECK(r.code.contains("#include \"kes_common.slang\""));
    CHECK(r.code.contains("VSOutput vsMain(MeshVertexInput vertex)"));
    CHECK(r.code.contains("float4 psMain(VSOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target"));
    CHECK(r.code.contains("uEmissiveTexture.Sample(input.uv).rgb"));
    CHECK(r.code.contains("sin(KesTime())"));
    CHECK(r.code.contains("uCameraPosition.xyz - input.worldPos"));
    CHECK(r.code.contains("static const float kProp_glow = 2.5;"));
    CHECK(r.code.contains("surface.emissive = n"));
    CHECK(r.code.contains("surface.normal = KesTangentNormal(surfaceInput, n"));
    CHECK(r.code.contains("return KesShadeMaterial(surfaceInput, surface);"));
    CHECK(!r.code.contains("surface.baseColor ="));
    CHECK(!r.code.contains("gScene"));
    CHECK(!r.code.contains("gMaterial"));
    CHECK(!r.code.contains("gSampler"));
    CHECK(r.warnings.join('\n').contains("Emissive slot has no texture"));

    CHECK(!SlangCodeGen::generate(m).ok);   // the Material node has no standalone form

    GraphModel plain;
    const int plainOut = plain.addNode("fragmentOutput", {});
    CHECK(!SlangCodeGen::hasSurfaceOverrides(plain));
    plain.setInputDefault(plainOut, "Color", {1.0, 0.0, 0.0});
    CHECK(SlangCodeGen::hasSurfaceOverrides(plain));
    CHECK(SlangCodeGen::generate(plain, "t", ShaderTarget::KesEngine).code.contains("surface.baseColor = float3(1.0, 0.0, 0.0);"));
}

static void testMaterialInGraphFile()
{
    GraphModel m;
    m.edit("add", [&] { m.addNode("fragmentOutput", {}); });

    MaterialSettings s = m.material();
    s.roughness = 0.25;
    s.transparent = true;
    s.textures[SlotNormal] = "textures/n.png";
    m.edit("material", [&] { m.setMaterial(s); });

    GraphModel loaded;
    QStringList warnings;
    CHECK(loaded.loadJson(m.toJson(), &warnings));
    CHECK(loaded.nodeCount() == 1);
    CHECK(loaded.material().roughness == 0.25);
    CHECK(loaded.material().transparent);
    CHECK(loaded.material().textures[SlotNormal] == "textures/n.png");

    m.undoStack()->undo();
    CHECK(m.material().roughness == 0.8);
    CHECK(m.nodeCount() == 1);
    m.undoStack()->redo();
    CHECK(m.material().roughness == 0.25);

    const QList<int> ids = m.nodeIds();
    CHECK(!m.toJson(&ids).contains("material"));
}

static QString readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

static void testMaterialExport()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString root = QDir(dir.path()).absoluteFilePath("content");
    const QString matPath = root + "/materials/Glow Rock.mat";
    QDir().mkpath(root + "/materials");
    {
        QFile project(dir.filePath("game.kproject"));
        CHECK(project.open(QIODevice::WriteOnly));
        project.write("Project\n{\n    Name=\"X\"\n    AssetRoot=\"content\"\n}\n");
    }
    CHECK(QDir::cleanPath(findContentRoot(matPath)) == QDir::cleanPath(root));
    CHECK(contentRelativePath(root, root + "/textures/a.png") == "textures/a.png");
    CHECK(contentRelativePath(root, dir.filePath("a.png")).isEmpty());

    GraphModel m;
    const int out = m.addNode("fragmentOutput", {});

    MaterialFiles files;
    QString error;
    CHECK(buildMaterialFiles(m, matPath, root, files, &error));
    CHECK(!files.hasShader());
    CHECK(!files.matText.contains("Shader ="));

    CHECK(!buildMaterialFiles(m, dir.filePath("outside.mat"), root, files, &error));

    const int noise = m.addNode("valueNoise", {});
    connectByName(m, noise, "Out", out, "Roughness");
    MaterialSettings s = m.material();
    s.metallic = 1.0;
    s.emissiveIntensity = 4.0;
    s.doubleSided = true;
    s.textures[SlotBaseColor] = "textures/rock.png";
    s.textures[SlotEmissive] = "textures/glow.png";
    m.setMaterial(s);

    CHECK(buildMaterialFiles(m, matPath, root, files, &error));
    CHECK(files.hasShader());
    CHECK(files.matText.contains("Shader = \"shaders/Glow_Rock.shader\""));
    CHECK(files.matText.contains("BaseTexture = \"textures/rock.png\""));
    CHECK(files.matText.contains("EmissiveTexture = \"textures/glow.png\""));
    CHECK(files.matText.contains("Metallic = 1\n"));
    CHECK(files.matText.contains("EmissiveIntensity = 4\n"));
    CHECK(files.matText.contains("DoubleSided = true\n"));
    CHECK(files.shaderText.contains("Source = \"shaders/Glow_Rock.slang\""));
    CHECK(handWrittenFiles(files).isEmpty());

    CHECK(writeMaterialFiles(files, &error));
    CHECK(readAll(matPath) == files.matText);
    CHECK(readAll(root + "/shaders/Glow_Rock.slang") == files.slangText);
    CHECK(handWrittenFiles(files).isEmpty());

    {
        QFile f(files.slangPath);
        CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("// my own shader\n");
    }
    CHECK(handWrittenFiles(files).contains(files.slangPath));
}

static void buildKitchenSink(GraphModel& m, int variant)
{
    const int out = m.addNode("fragmentOutput", {});
    QVector<int> nodes;
    for (const NodeDef& d : NodeLibrary::instance().all()) {
        if (d.role == NodeRole::FragmentOutput || d.engineOnly) continue;
        nodes << m.addNode(d.type, {});
    }

    int src = -1;
    const char* srcPort = nullptr;
    if (variant == 1) { src = m.addNode("color", {}); srcPort = "RGBA"; }
    if (variant == 2) { src = m.addNode("uv", {}); srcPort = "UV"; }
    if (variant == 3) { src = m.addNode("time", {}); srcPort = "Time"; }
    if (src >= 0) {
        for (int id : nodes) {
            const NodeDef* d = m.def(id);
            if (d->role == NodeRole::CustomExpression) continue;
            for (int i = 0; i < d->inputs.size(); ++i)
                m.addConnection(src, m.def(src)->outputIndex(srcPort), id, i);
        }
    }

    int prev = -1;
    for (int id : nodes) {
        const NodeDef* d = m.def(id);
        for (int o = 0; o < d->outputs.size(); ++o) {
            const int c = m.addNode("customExpression", {});
            m.setParam(c, "Expression", "B + 0.0");
            m.addConnection(id, o, c, 0);
            if (prev >= 0) m.addConnection(prev, 0, c, 1);
            prev = c;
        }
    }
    m.addConnection(prev, 0, out, 2);
}

static void testKitchenSink(const QString& dumpDir)
{
    for (int variant = 0; variant < 4; ++variant) {
        GraphModel m;
        buildKitchenSink(m, variant);
        CodeGenResult r = SlangCodeGen::generate(m);
        CHECK(r.ok);
        CHECK(r.warnings.isEmpty());
        if (!dumpDir.isEmpty()) {
            QFile f(QString("%1/kitchen%2.slang").arg(dumpDir).arg(variant));
            if (f.open(QIODevice::WriteOnly)) f.write(r.code.toUtf8());
        }
    }
}

static void testNoOutput()
{
    GraphModel m;
    m.addNode("uv", {});
    CodeGenResult r = SlangCodeGen::generate(m);
    CHECK(!r.ok);
    CHECK(!r.errors.isEmpty());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testConversions();
    testModelAndUndo();
    testCycleRejection();
    testTypeInference();
    testJsonRoundTrip();
    testPasteRemapsIds();
    testCodegen(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString());
    testPropertiesTexturesCustom();
    testEngineTarget();
    testMaterialInGraphFile();
    testMaterialExport();
    testKitchenSink(argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString());
    testNoOutput();

    if (g_failures == 0) std::cout << "All tests passed" << std::endl;
    else std::cerr << g_failures << " check(s) failed" << std::endl;
    return g_failures == 0 ? 0 : 1;
}

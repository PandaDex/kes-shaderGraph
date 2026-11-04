#include "ExampleGraphs.h"

namespace sne {

namespace {

void link(GraphModel& m, int from, const char* fromPort, int to, const char* toPort)
{
    const NodeDef* df = m.def(from);
    const NodeDef* dt = m.def(to);
    if (!df || !dt) return;
    m.addConnection(from, df->outputIndex(fromPort), to, dt->inputIndex(toPort));
}

} // namespace

void buildEmptyGraph(GraphModel& m)
{
    m.clear();
    m.addNode("fragmentOutput", QPointF(0, 0));
    emit m.graphReset();
    emit m.changed();
}

void buildExampleGraph(GraphModel& m)
{
    m.blockSignals(true);
    m.clear();

    const int uv = m.addNode("uv", QPointF(-1060, -140));
    const int time = m.addNode("time", QPointF(-1060, 40));
    const int speed = m.addNode("multiply", QPointF(-840, 60));
    const int combine = m.addNode("combine", QPointF(-620, 40));
    const int tiling = m.addNode("tilingOffset", QPointF(-400, -120));
    const int fbm = m.addNode("fbm", QPointF(-160, -120));
    const int colA = m.addNode("color", QPointF(-160, 150));
    const int colB = m.addNode("color", QPointF(-160, 300));
    const int mix = m.addNode("lerp", QPointF(120, 40));
    const int fres = m.addNode("fresnel", QPointF(120, 290));
    const int rim = m.addNode("multiply", QPointF(360, 260));
    const int out = m.addNode("fragmentOutput", QPointF(620, 60));

    m.setInputDefault(speed, "B", {0.08});
    m.setInputDefault(tiling, "Tiling", {6.0, 6.0});
    m.setInputDefault(fbm, "Scale", {1.0});
    m.setInputDefault(fbm, "Octaves", {5});
    m.setParam(colA, "Color", QVariantList{0.04, 0.07, 0.28, 1.0});
    m.setParam(colB, "Color", QVariantList{1.0, 0.45, 0.10, 1.0});
    m.setInputDefault(fres, "Power", {3.0});

    link(m, time, "Time", speed, "A");
    link(m, speed, "Out", combine, "R");
    link(m, uv, "UV", tiling, "UV");
    link(m, combine, "RG", tiling, "Offset");
    link(m, tiling, "Out", fbm, "UV");
    link(m, colA, "RGB", mix, "A");
    link(m, colB, "RGB", mix, "B");
    link(m, fbm, "Out", mix, "T");
    link(m, fres, "Out", rim, "A");
    link(m, colB, "RGB", rim, "B");
    link(m, mix, "Out", out, "Color");
    link(m, rim, "Out", out, "Emission");

    m.blockSignals(false);
    emit m.graphReset();
    emit m.changed();
}

} // namespace sne

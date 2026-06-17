#pragma once

#include "../ast/DiagramAst.h"
#include "../layout_graph/LayoutGraph.h"
#include "../render_model/RenderDocument.h"
#include "../render_model/RenderTheme.h"
#include "DotBuilder.h"
#include "DotJsonParser.h"
#include "DotProcessRunner.h"

class GraphvizLayoutEngine
{
public:
    RenderDocument layout(const DiagramAst &ast, const RenderTheme &theme);

private:
    LayoutGraph buildSequenceGraph(const SequenceDiagramAst &ast) const;
    LayoutGraph buildClassGraph(const ClassDiagramAst &ast) const;
    RenderDocument buildRenderDocument(
        const DiagramAst &ast,
        const LayoutGraph &layoutGraph,
        const GraphLayoutResult &layoutResult) const;

    QSizeF measureParticipant(const ParticipantDecl &participant) const;
    QSizeF measureClass(const ClassDecl &klass) const;
    RenderEdgeKind relationKindToRenderKind(RelationKind kind) const;
    QPainterPath pathFromPoints(const QVector<QPointF> &points) const;
};

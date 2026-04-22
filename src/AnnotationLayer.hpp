#pragma once
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QPainter>
#include <QFont>
#include <cmath>

// Non-destructive annotation layer rendered over the image.
// All coordinates are in image (pixel) space.
class AnnotationLayer {
public:
    enum class DrawTool { Arrow, Rectangle, Ellipse, Text, Freehand, Eraser };

    // ── Tool / property setters ───────────────────────────────────────────────
    void setDrawTool(DrawTool t) { m_drawTool = t; }
    DrawTool drawTool() const    { return m_drawTool; }
    void setColor(QColor c)      { m_color = c; }
    QColor color() const         { return m_color; }
    void setLineWidth(int w)     { m_lineWidth = std::max(1, w); }
    int  lineWidth() const       { return m_lineWidth; }
    void setFontSize(int s)      { m_fontSize = std::max(6, s); }
    int  fontSize() const        { return m_fontSize; }

    // ── Mouse event API (image coordinates) ──────────────────────────────────
    void pressAt(QPointF pt);
    void moveAt(QPointF pt);
    // text is only used by the Text tool (caller fetches it from QInputDialog)
    void releaseAt(QPointF pt, const QString& text = {});

    // ── Undo / redo ───────────────────────────────────────────────────────────
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    // ── Rendering ─────────────────────────────────────────────────────────────
    // Render all annotations to an ARGB32 image (image coordinates).
    QImage renderToImage(int width, int height) const;

    // Composite annotations onto base (returns new ARGB32 or RGB888 image).
    QImage flattenOnto(const QImage& base) const;

    // ── Persistence ───────────────────────────────────────────────────────────
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& obj);

    bool isEmpty() const { return m_items.isEmpty() && !m_drawing; }
    void clear();

    struct Item {
        enum Type { Arrow, Rect, Ellipse, Text, Freehand };
        Type             type      = Arrow;
        QColor           color     = Qt::red;
        int              lineWidth = 2;
        int              fontSize  = 14;
        QPointF          p1, p2;
        QVector<QPointF> points;
        QString          text;
    };

private:
    void paintItem(QPainter& p, const Item& item, double scale = 1.0) const;
    static void drawArrowHead(QPainter& p, QPointF a, QPointF b, double size);
    bool hitTest(const Item& item, QPointF pt, double radius) const;
    void commitCurrent();
    void pushHistory();

    DrawTool m_drawTool = DrawTool::Arrow;
    QColor   m_color    = Qt::red;
    int      m_lineWidth = 2;
    int      m_fontSize  = 14;

    QVector<Item> m_items;
    Item  m_current;
    bool  m_drawing = false;

    // Undo/redo: snapshots of m_items
    QVector<QVector<Item>> m_history;
    int m_histPos = -1;
    static constexpr int kMaxHistory = 30;
};

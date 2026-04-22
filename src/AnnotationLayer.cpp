#include "AnnotationLayer.hpp"
#include "Logger.hpp"
#include <QJsonArray>
#include <QJsonValue>
#include <QPainterPath>
#include <algorithm>

// ── History helpers ───────────────────────────────────────────────────────────

void AnnotationLayer::pushHistory() {
    // Discard any redo states
    if (m_histPos < m_history.size() - 1)
        m_history.erase(m_history.begin() + m_histPos + 1, m_history.end());
    m_history.push_back(m_items);
    m_histPos = m_history.size() - 1;
    // Trim old history
    if (m_history.size() > kMaxHistory) {
        m_history.remove(0);
        m_histPos = m_history.size() - 1;
    }
}

bool AnnotationLayer::canUndo() const { return m_histPos > 0; }
bool AnnotationLayer::canRedo() const { return m_histPos < m_history.size() - 1; }

void AnnotationLayer::undo() {
    if (!canUndo()) return;
    --m_histPos;
    m_items = m_history[m_histPos];
}

void AnnotationLayer::redo() {
    if (!canRedo()) return;
    ++m_histPos;
    m_items = m_history[m_histPos];
}

void AnnotationLayer::clear() {
    m_items.clear();
    m_drawing = false;
    m_history.clear();
    m_histPos = -1;
    // Push clean state
    m_history.push_back({});
    m_histPos = 0;
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void AnnotationLayer::pressAt(QPointF pt) {
    if (m_drawTool == DrawTool::Eraser) {
        // Erase top-most annotation within 15px
        for (int i = m_items.size() - 1; i >= 0; --i) {
            if (hitTest(m_items[i], pt, 15.0)) {
                pushHistory();
                m_items.remove(i);
                return;
            }
        }
        return;
    }
    m_drawing = true;
    m_current = {};
    m_current.color     = m_color;
    m_current.lineWidth = m_lineWidth;
    m_current.fontSize  = m_fontSize;
    m_current.p1 = m_current.p2 = pt;

    switch (m_drawTool) {
        case DrawTool::Arrow:      m_current.type = Item::Arrow;   break;
        case DrawTool::Rectangle:  m_current.type = Item::Rect;    break;
        case DrawTool::Ellipse:    m_current.type = Item::Ellipse; break;
        case DrawTool::Text:       m_current.type = Item::Text;    break;
        case DrawTool::Freehand:
            m_current.type = Item::Freehand;
            m_current.points.append(pt);
            break;
        default: break;
    }
}

void AnnotationLayer::moveAt(QPointF pt) {
    if (!m_drawing) return;
    m_current.p2 = pt;
    if (m_current.type == Item::Freehand)
        m_current.points.append(pt);
}

void AnnotationLayer::releaseAt(QPointF pt, const QString& text) {
    if (!m_drawing) return;
    m_current.p2 = pt;
    if (m_current.type == Item::Freehand)
        m_current.points.append(pt);
    if (m_current.type == Item::Text)
        m_current.text = text;
    m_drawing = false;

    // Only commit if the annotation is non-trivial
    bool valid = false;
    if (m_current.type == Item::Freehand)
        valid = m_current.points.size() > 1;
    else if (m_current.type == Item::Text)
        valid = !text.isEmpty();
    else {
        double dx = m_current.p2.x() - m_current.p1.x();
        double dy = m_current.p2.y() - m_current.p1.y();
        valid = (std::hypot(dx, dy) > 2.0);
    }

    if (valid) {
        pushHistory();
        m_items.append(m_current);
    }
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void AnnotationLayer::drawArrowHead(QPainter& p, QPointF a, QPointF b, double sz) {
    QPointF d = b - a;
    double len = std::hypot(d.x(), d.y());
    if (len < 1.0) return;
    d /= len;
    QPointF n(-d.y(), d.x());
    QPolygonF head;
    head << b
         << (b - d * sz + n * sz * 0.4)
         << (b - d * sz - n * sz * 0.4);
    p.setBrush(p.pen().color());
    p.drawPolygon(head);
    p.setBrush(Qt::NoBrush);
}

void AnnotationLayer::paintItem(QPainter& p, const Item& item, double /*scale*/) const {
    QPen pen(item.color, item.lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (item.type) {
        case Item::Arrow:
            p.drawLine(item.p1, item.p2);
            drawArrowHead(p, item.p1, item.p2, item.lineWidth * 4.0 + 8.0);
            break;
        case Item::Rect:
            p.drawRect(QRectF(item.p1, item.p2).normalized());
            break;
        case Item::Ellipse:
            p.drawEllipse(QRectF(item.p1, item.p2).normalized());
            break;
        case Item::Text: {
            QFont f("Arial", item.fontSize);
            p.setFont(f);
            p.setPen(item.color);
            p.drawText(item.p1, item.text);
            break;
        }
        case Item::Freehand:
            if (item.points.size() > 1)
                p.drawPolyline(item.points.data(), item.points.size());
            break;
    }
}

QImage AnnotationLayer::renderToImage(int width, int height) const {
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    for (const auto& item : m_items)
        paintItem(p, item);

    if (m_drawing)
        paintItem(p, m_current);

    return img;
}

QImage AnnotationLayer::flattenOnto(const QImage& base) const {
    QImage result = base.convertToFormat(QImage::Format_ARGB32);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    for (const auto& item : m_items)
        paintItem(p, item);
    return result;
}

// ── Hit test ─────────────────────────────────────────────────────────────────

static double pointToSegmentDist(QPointF p, QPointF a, QPointF b) {
    QPointF ab = b - a, ap = p - a;
    double t = (ab.x()*ap.x() + ab.y()*ap.y()) / (ab.x()*ab.x() + ab.y()*ab.y() + 1e-9);
    t = std::clamp(t, 0.0, 1.0);
    QPointF closest = a + t * ab;
    QPointF d = p - closest;
    return std::hypot(d.x(), d.y());
}

bool AnnotationLayer::hitTest(const Item& item, QPointF pt, double radius) const {
    switch (item.type) {
        case Item::Arrow:
        case Item::Freehand: {
            const auto& pts = (item.type == Item::Arrow)
                ? QVector<QPointF>{item.p1, item.p2}
                : item.points;
            for (int i = 0; i + 1 < pts.size(); i++)
                if (pointToSegmentDist(pt, pts[i], pts[i+1]) <= radius) return true;
            return false;
        }
        case Item::Rect: {
            QRectF r = QRectF(item.p1, item.p2).normalized().adjusted(-radius,-radius,radius,radius);
            return r.contains(pt);
        }
        case Item::Ellipse: {
            QRectF r = QRectF(item.p1, item.p2).normalized();
            QPointF c = r.center();
            double rx = r.width() / 2 + radius, ry = r.height() / 2 + radius;
            if (rx < 1 || ry < 1) return false;
            double dx = (pt.x() - c.x()) / rx, dy = (pt.y() - c.y()) / ry;
            return (dx*dx + dy*dy) <= 1.0;
        }
        case Item::Text: {
            QFontMetricsF fm(QFont("Arial", item.fontSize));
            QRectF br = fm.boundingRect(item.text).translated(item.p1);
            return br.adjusted(-radius,-radius,radius,radius).contains(pt);
        }
    }
    return false;
}

// ── JSON ─────────────────────────────────────────────────────────────────────

static QString typeToStr(AnnotationLayer::Item::Type t) {
    switch (t) {
        case AnnotationLayer::Item::Arrow:   return "arrow";
        case AnnotationLayer::Item::Rect:    return "rect";
        case AnnotationLayer::Item::Ellipse: return "ellipse";
        case AnnotationLayer::Item::Text:    return "text";
        case AnnotationLayer::Item::Freehand:return "freehand";
    }
    return "unknown";
}

static AnnotationLayer::Item::Type strToType(const QString& s) {
    if (s == "arrow")    return AnnotationLayer::Item::Arrow;
    if (s == "rect")     return AnnotationLayer::Item::Rect;
    if (s == "ellipse")  return AnnotationLayer::Item::Ellipse;
    if (s == "text")     return AnnotationLayer::Item::Text;
    return AnnotationLayer::Item::Freehand;
}

static QJsonObject ptToJson(QPointF p) {
    return QJsonObject{ {"x", p.x()}, {"y", p.y()} };
}
static QPointF jsonToPt(const QJsonObject& o) {
    return { o["x"].toDouble(), o["y"].toDouble() };
}

QJsonObject AnnotationLayer::toJson() const {
    QJsonArray arr;
    for (const auto& item : m_items) {
        QJsonObject obj;
        obj["type"]      = typeToStr(item.type);
        obj["color"]     = item.color.name(QColor::HexArgb);
        obj["lineWidth"] = item.lineWidth;
        obj["fontSize"]  = item.fontSize;
        obj["p1"]        = ptToJson(item.p1);
        obj["p2"]        = ptToJson(item.p2);
        obj["text"]      = item.text;
        QJsonArray pts;
        for (const auto& p : item.points) pts.append(ptToJson(p));
        obj["points"] = pts;
        arr.append(obj);
    }
    return QJsonObject{ {"annotations", arr} };
}

bool AnnotationLayer::fromJson(const QJsonObject& root) {
    m_items.clear();
    QJsonArray arr = root["annotations"].toArray();
    LOG_INFO("AnnotationLayer",
        QString("Loading %1 annotation(s) from JSON").arg(arr.size()));
    for (const auto& v : arr) {
        QJsonObject obj = v.toObject();
        Item item;
        item.type      = strToType(obj["type"].toString());
        item.color     = QColor(obj["color"].toString());
        item.lineWidth = obj["lineWidth"].toInt(2);
        item.fontSize  = obj["fontSize"].toInt(14);
        item.p1        = jsonToPt(obj["p1"].toObject());
        item.p2        = jsonToPt(obj["p2"].toObject());
        item.text      = obj["text"].toString();
        for (const auto& pv : obj["points"].toArray())
            item.points.append(jsonToPt(pv.toObject()));
        m_items.append(item);
    }
    // Reset history with loaded state
    m_history.clear();
    m_history.push_back(m_items);
    m_histPos = 0;
    return true;
}

#pragma once
#include <QLabel>
#include <QRubberBand>
#include <QPoint>

class QScrollArea;

// QLabel subclass that supports:
//   - rubber-band crop selection (Tool::Crop)
//   - click-and-drag pan when crop is inactive (Tool::None)
class ImageLabel : public QLabel {
    Q_OBJECT
public:
    enum class Tool { None, Crop };

    explicit ImageLabel(QWidget* parent = nullptr);

    void setTool(Tool t);
    Tool tool() const { return m_tool; }

    // Must be called once after the scroll area is set up so pan can move its bars.
    void setScrollArea(QScrollArea* sa) { m_scrollArea = sa; }

    // Set so the label can convert display coords → image coords
    void setDisplayZoom(double zoom) { m_zoom = zoom; }

signals:
    // Emitted in image coordinates when the user finishes drawing a rect
    void cropSelected(QRect imageRect);

protected:
    void enterEvent(QEnterEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    Tool         m_tool        = Tool::None;
    double       m_zoom        = 1.0;

    // ── Crop ──────────────────────────────────────────────────────────────
    QPoint       m_cropOrigin;
    QRubberBand* m_rubberBand  = nullptr;

    // ── Pan ───────────────────────────────────────────────────────────────
    QScrollArea* m_scrollArea  = nullptr;
    bool         m_panning     = false;
    QPoint       m_panOrigin;          // global cursor pos at pan start
    int          m_scrollHStart = 0;   // scrollbar value at pan start
    int          m_scrollVStart = 0;
};

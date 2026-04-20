#pragma once
#include <QLabel>
#include <QRubberBand>
#include <QPoint>

// QLabel subclass responsible solely for rubber-band crop selection.
// Pan is handled by PanHandler installed on the parent scroll area.
class ImageLabel : public QLabel {
    Q_OBJECT
public:
    enum class Tool { None, Crop };

    explicit ImageLabel(QWidget* parent = nullptr);

    void setTool(Tool t);
    Tool tool() const { return m_tool; }

    // Set so the label can convert display coords → image coords
    void setDisplayZoom(double zoom) { m_zoom = zoom; }

signals:
    // Emitted in image coordinates when the user finishes drawing a rect
    void cropSelected(QRect imageRect);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    Tool         m_tool       = Tool::None;
    double       m_zoom       = 1.0;
    QPoint       m_cropOrigin;
    QRubberBand* m_rubberBand = nullptr;
};

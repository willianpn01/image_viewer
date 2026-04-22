#pragma once
#include <QLabel>
#include <QRubberBand>
#include <QPoint>
#include <QImage>
#include <QPainter>

class ImageLabel : public QLabel {
    Q_OBJECT
public:
    enum class Tool { None, Crop, MagicSelect, Annotate };

    explicit ImageLabel(QWidget* parent = nullptr);

    void setTool(Tool t);
    Tool tool() const { return m_tool; }

    void setDisplayZoom(double zoom) { m_zoom = zoom; }

    // Selection overlay (image coordinates, ARGB32)
    void setSelectionOverlay(const QImage& overlay);
    void clearSelectionOverlay();

    // Annotation overlay (image coordinates, ARGB32) — Feature 2
    void setAnnotationOverlay(const QImage& overlay);
    void clearAnnotationOverlay();

signals:
    void cropSelected(QRect imageRect);
    void pixelClicked(QPoint imagePoint);     // MagicSelect: image-coord click
    void annotatePress(QPointF imagePoint);   // Annotate tool signals (float coords)
    void annotateMove(QPointF imagePoint);
    void annotateRelease(QPointF imagePoint);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    Tool         m_tool       = Tool::None;
    double       m_zoom       = 1.0;
    QPoint       m_cropOrigin;
    QRubberBand* m_rubberBand = nullptr;

    QImage m_selectionOverlay;
    QImage m_annotationOverlay;
};

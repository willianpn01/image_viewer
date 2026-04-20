#include "ImageLabel.hpp"
#include <QMouseEvent>

ImageLabel::ImageLabel(QWidget* parent) : QLabel(parent) {
    setMouseTracking(true);
}

void ImageLabel::setTool(Tool t) {
    m_tool = t;
    if (t == Tool::Crop)
        setCursor(Qt::CrossCursor);
    else
        setCursor(Qt::ArrowCursor);

    if (m_rubberBand) {
        m_rubberBand->hide();
    }
}

void ImageLabel::mousePressEvent(QMouseEvent* e) {
    if (m_tool == Tool::Crop && e->button() == Qt::LeftButton) {
        m_origin = e->pos();
        if (!m_rubberBand) m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        m_rubberBand->setGeometry(QRect(m_origin, QSize()));
        m_rubberBand->show();
    }
    QLabel::mousePressEvent(e);
}

void ImageLabel::mouseMoveEvent(QMouseEvent* e) {
    if (m_tool == Tool::Crop && m_rubberBand && (e->buttons() & Qt::LeftButton))
        m_rubberBand->setGeometry(QRect(m_origin, e->pos()).normalized());
    QLabel::mouseMoveEvent(e);
}

void ImageLabel::mouseReleaseEvent(QMouseEvent* e) {
    if (m_tool == Tool::Crop && e->button() == Qt::LeftButton && m_rubberBand) {
        QRect displayRect = QRect(m_origin, e->pos()).normalized();
        m_rubberBand->hide();
        setTool(Tool::None); // auto-reset

        if (displayRect.width() > 4 && displayRect.height() > 4) {
            // Convert from display (label) coordinates to image coordinates
            // The label has the pixmap centred, so find pixmap offset
            QPixmap pm = pixmap();
            if (!pm.isNull()) {
                int offX = (width()  - pm.width())  / 2;
                int offY = (height() - pm.height()) / 2;
                QRect imgCoords(
                    (int)((displayRect.x() - offX) / m_zoom),
                    (int)((displayRect.y() - offY) / m_zoom),
                    (int)(displayRect.width()  / m_zoom),
                    (int)(displayRect.height() / m_zoom)
                );
                emit cropSelected(imgCoords);
            }
        }
    }
    QLabel::mouseReleaseEvent(e);
}

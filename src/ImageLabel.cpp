#include "ImageLabel.hpp"
#include <QMouseEvent>
#include <QEnterEvent>
#include <QScrollArea>
#include <QScrollBar>

ImageLabel::ImageLabel(QWidget* parent) : QLabel(parent) {
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
}

void ImageLabel::setTool(Tool t) {
    m_tool    = t;
    m_panning = false;

    if (t == Tool::Crop)
        setCursor(Qt::CrossCursor);
    else
        setCursor(Qt::OpenHandCursor);

    if (m_rubberBand) m_rubberBand->hide();
}

// ── Enter ─────────────────────────────────────────────────────────────────────

void ImageLabel::enterEvent(QEnterEvent* e) {
    if (m_tool != Tool::Crop)
        setCursor(Qt::OpenHandCursor);
    QLabel::enterEvent(e);
}

// ── Press ─────────────────────────────────────────────────────────────────────

void ImageLabel::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (m_tool == Tool::Crop) {
            // Start rubber-band
            m_cropOrigin = e->pos();
            if (!m_rubberBand)
                m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
            m_rubberBand->setGeometry(QRect(m_cropOrigin, QSize()));
            m_rubberBand->show();
        } else if (m_scrollArea) {
            // Start pan
            m_panning      = true;
            m_panOrigin    = e->globalPosition().toPoint();
            m_scrollHStart = m_scrollArea->horizontalScrollBar()->value();
            m_scrollVStart = m_scrollArea->verticalScrollBar()->value();
            setCursor(Qt::ClosedHandCursor);
        }
    }
    QLabel::mousePressEvent(e);
}

// ── Move ──────────────────────────────────────────────────────────────────────

void ImageLabel::mouseMoveEvent(QMouseEvent* e) {
    if (m_tool == Tool::Crop && m_rubberBand && (e->buttons() & Qt::LeftButton)) {
        m_rubberBand->setGeometry(QRect(m_cropOrigin, e->pos()).normalized());
    } else if (m_panning && m_scrollArea && (e->buttons() & Qt::LeftButton)) {
        QPoint delta = e->globalPosition().toPoint() - m_panOrigin;
        m_scrollArea->horizontalScrollBar()->setValue(m_scrollHStart - delta.x());
        m_scrollArea->verticalScrollBar()->setValue(m_scrollVStart - delta.y());
    }
    QLabel::mouseMoveEvent(e);
}

// ── Release ───────────────────────────────────────────────────────────────────

void ImageLabel::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (m_tool == Tool::Crop && m_rubberBand) {
            QRect displayRect = QRect(m_cropOrigin, e->pos()).normalized();
            m_rubberBand->hide();
            setTool(Tool::None); // auto-reset to pan mode

            if (displayRect.width() > 4 && displayRect.height() > 4) {
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
        } else if (m_panning) {
            m_panning = false;
            setCursor(Qt::OpenHandCursor);
        }
    }
    QLabel::mouseReleaseEvent(e);
}

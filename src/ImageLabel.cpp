#include "ImageLabel.hpp"
#include <QMouseEvent>
#include <QEnterEvent>
#include <QScrollArea>
#include <QScrollBar>

// ROOT-CAUSE NOTE:
// QLabel::mousePressEvent calls e->ignore() for pixmap-mode labels
// (QLabel only accepts events when it has selectable text).  An ignored
// press propagates to the parent viewport, which lets QAbstractScrollArea
// steal the mouse grab — so mouseMoveEvent is never delivered to us during
// a drag.  Fix: always call e->accept() and return ourselves when handling
// crop or pan; never fall through to the base-class call in those paths.

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
            // ── Crop: start rubber-band ────────────────────────────────────
            m_cropOrigin = e->pos();
            if (!m_rubberBand)
                m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
            m_rubberBand->setGeometry(QRect(m_cropOrigin, QSize()));
            m_rubberBand->show();
            e->accept();   // keep mouse grab on this widget
            return;        // ← do NOT call QLabel::mousePressEvent

        } else if (m_scrollArea) {
            // ── Pan: capture origin + current scroll position ──────────────
            m_panning      = true;
            m_panOrigin    = e->globalPosition().toPoint();
            m_scrollHStart = m_scrollArea->horizontalScrollBar()->value();
            m_scrollVStart = m_scrollArea->verticalScrollBar()->value();
            setCursor(Qt::ClosedHandCursor);
            e->accept();   // keep mouse grab — prevents viewport from stealing it
            return;        // ← do NOT call QLabel::mousePressEvent
        }
    }
    // For all other button combinations fall through to the base class.
    QLabel::mousePressEvent(e);
}

// ── Move ──────────────────────────────────────────────────────────────────────

void ImageLabel::mouseMoveEvent(QMouseEvent* e) {
    if (m_tool == Tool::Crop && m_rubberBand && (e->buttons() & Qt::LeftButton)) {
        m_rubberBand->setGeometry(QRect(m_cropOrigin, e->pos()).normalized());
        e->accept();
        return;
    }

    if (m_panning && m_scrollArea && (e->buttons() & Qt::LeftButton)) {
        QPoint delta = e->globalPosition().toPoint() - m_panOrigin;
        m_scrollArea->horizontalScrollBar()->setValue(m_scrollHStart - delta.x());
        m_scrollArea->verticalScrollBar()->setValue(m_scrollVStart - delta.y());
        e->accept();
        return;
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
            e->accept();
            return;
        }

        if (m_panning) {
            m_panning = false;
            setCursor(Qt::OpenHandCursor);
            e->accept();
            return;
        }
    }
    QLabel::mouseReleaseEvent(e);
}

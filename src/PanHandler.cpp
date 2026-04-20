#include "PanHandler.hpp"
#include <QScrollArea>
#include <QScrollBar>
#include <QMouseEvent>
#include <QEvent>
#include <QWidget>
#include <QCursor>

PanHandler::PanHandler(QScrollArea* sa, QWidget* imgLabel, QObject* parent)
    : QObject(parent), m_sa(sa), m_imgLabel(imgLabel)
{
    // Intercept events on the viewport (gray area around image when image is
    // smaller than the viewport) AND on the image label itself.
    sa->viewport()->installEventFilter(this);
    imgLabel->installEventFilter(this);

    // Default to pan cursor on both surfaces.
    applyCursor(Qt::OpenHandCursor);
}

void PanHandler::setCropActive(bool active) {
    m_cropActive = active;
    if (!active) {
        m_panning = false;
        applyCursor(Qt::OpenHandCursor);
    }
    // When crop becomes active the ImageLabel sets CrossCursor itself;
    // we just stay out of the way.
}

void PanHandler::applyCursor(Qt::CursorShape shape) {
    m_sa->viewport()->setCursor(shape);
    m_imgLabel->setCursor(shape);
}

bool PanHandler::eventFilter(QObject* obj, QEvent* event) {
    // Only handle events from our two watched objects.
    if (obj != m_sa->viewport() && obj != m_imgLabel)
        return false;

    // While crop is active let every event through unmodified.
    if (m_cropActive)
        return false;

    switch (event->type()) {

    // ── Press ─────────────────────────────────────────────────────────────────
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_panning     = true;
            m_lastGlobal  = me->globalPosition().toPoint();
            applyCursor(Qt::ClosedHandCursor);
            return true;   // consumed — do not pass to child widget
        }
        break;
    }

    // ── Move ──────────────────────────────────────────────────────────────────
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_panning && (me->buttons() & Qt::LeftButton)) {
            QPoint cur   = me->globalPosition().toPoint();
            QPoint delta = cur - m_lastGlobal;
            m_lastGlobal = cur;

            m_sa->horizontalScrollBar()->setValue(
                m_sa->horizontalScrollBar()->value() - delta.x());
            m_sa->verticalScrollBar()->setValue(
                m_sa->verticalScrollBar()->value() - delta.y());
            return true;
        }
        // Button was released without a release event (e.g., left the window).
        if (m_panning) {
            m_panning = false;
            applyCursor(Qt::OpenHandCursor);
        }
        break;
    }

    // ── Release ───────────────────────────────────────────────────────────────
    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_panning) {
            m_panning = false;
            applyCursor(Qt::OpenHandCursor);
            return true;
        }
        break;
    }

    default:
        break;
    }

    return false;
}

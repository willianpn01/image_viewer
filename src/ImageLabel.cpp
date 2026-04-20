#include "ImageLabel.hpp"
#include <QMouseEvent>
#include <QEnterEvent>

// Pan is handled entirely by PanHandler (event filter on the viewport).
// This class only handles the rubber-band crop selection.
//
// IMPORTANT: In Tool::None mode we call unsetCursor() so the cursor set by
// PanHandler on the viewport shines through (child widgets normally override
// the parent cursor — unsetCursor() cancels that override).

ImageLabel::ImageLabel(QWidget* parent) : QLabel(parent) {
    setMouseTracking(true);
}

void ImageLabel::setTool(Tool t) {
    m_tool = t;

    if (t == Tool::Crop)
        setCursor(Qt::CrossCursor);
    else
        unsetCursor();   // let the viewport's OpenHandCursor show through

    if (m_rubberBand) m_rubberBand->hide();
}

// ── Press ─────────────────────────────────────────────────────────────────────

void ImageLabel::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_tool == Tool::Crop) {
        m_cropOrigin = e->pos();
        if (!m_rubberBand)
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        m_rubberBand->setGeometry(QRect(m_cropOrigin, QSize()));
        m_rubberBand->show();
        e->accept();
        return;
    }
    // Pass everything else (including pan presses in Tool::None mode) to the
    // event filter on the parent viewport — do NOT call QLabel::mousePressEvent,
    // which would call e->ignore() for pixmap labels and let
    // QAbstractScrollArea steal the mouse grab before PanHandler gets it.
    e->ignore();
}

// ── Move ──────────────────────────────────────────────────────────────────────

void ImageLabel::mouseMoveEvent(QMouseEvent* e) {
    if (m_tool == Tool::Crop && m_rubberBand && (e->buttons() & Qt::LeftButton)) {
        m_rubberBand->setGeometry(QRect(m_cropOrigin, e->pos()).normalized());
        e->accept();
        return;
    }
    e->ignore();
}

// ── Release ───────────────────────────────────────────────────────────────────

void ImageLabel::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_tool == Tool::Crop && m_rubberBand) {
        QRect displayRect = QRect(m_cropOrigin, e->pos()).normalized();
        m_rubberBand->hide();
        setTool(Tool::None);   // auto-reset to pan mode

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
    e->ignore();
}

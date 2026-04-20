#pragma once
#include <QObject>
#include <QPoint>

class QScrollArea;
class QWidget;

// Installs itself as an event-filter on BOTH the viewport and the image label.
//
// Why both?
//   - Events on the image label (child of viewport) go straight to the label,
//     NOT through a filter on the viewport.
//   - Events on the gray area around the image go to the viewport directly.
// Installing on both objects covers the full drag surface.
//
// Global-position deltas are used so that switching between objects mid-drag
// never causes a jump.
//
class PanHandler : public QObject {
    Q_OBJECT
public:
    // imageLabel must already be a child of scrollArea->viewport().
    PanHandler(QScrollArea* scrollArea, QWidget* imageLabel, QObject* parent = nullptr);

public slots:
    // Call whenever crop mode is toggled.  While crop is active the handler
    // passes all events through unchanged.
    void setCropActive(bool active);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QScrollArea* m_sa;
    QWidget*     m_imgLabel;

    bool   m_panning    = false;
    bool   m_cropActive = false;
    QPoint m_lastGlobal;   // last known global cursor position during pan

    void applyCursor(Qt::CursorShape shape);
};

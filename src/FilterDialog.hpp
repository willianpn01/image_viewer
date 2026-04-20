#pragma once
#include <QDialog>
#include <QVector>
#include <QString>
#include <QLabel>
#include <QSlider>
#include <functional>
#include "ImageConvert.hpp"

// Generic filter/adjustment dialog with a live preview.
class FilterDialog : public QDialog {
    Q_OBJECT
public:
    struct Param {
        QString name;
        double  min;
        double  max;
        double  defaultVal;
        int     steps     = 200;   // slider resolution
        bool    isInt     = false;
        QString suffix    = "";
    };

    FilterDialog(
        const QString& title,
        const QVector<Param>& params,
        const cimg_library::CImg<unsigned char>& original,
        std::function<cimg_library::CImg<unsigned char>(
            const cimg_library::CImg<unsigned char>&,
            const QVector<double>&)> processor,
        QWidget* parent = nullptr);

    QVector<double> values() const { return m_values; }

private slots:
    void onSliderChanged(int idx, int sliderVal);

private:
    void updatePreview();

    QVector<Param>   m_params;
    QVector<double>  m_values;
    QVector<QSlider*> m_sliders;
    QVector<QLabel*>  m_valLabels;

    QLabel* m_previewLabel = nullptr;

    cimg_library::CImg<unsigned char> m_original;
    std::function<cimg_library::CImg<unsigned char>(
        const cimg_library::CImg<unsigned char>&,
        const QVector<double>&)> m_processor;
};

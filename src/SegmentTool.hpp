#pragma once
#include "ImageProcessor.hpp"
#include <vector>
#include <QImage>

// Region-growing segmentation (Magic Select tool)
class SegmentTool {
public:
    // BFS region growing from (seedX, seedY).
    // Returns flat mask (true = selected) of size width * height.
    static std::vector<bool> regionGrow(
        const CImgU8& img, int seedX, int seedY, int threshold);

    static int countSelected(const std::vector<bool>& mask);

    // Build a semi-transparent ARGB32 overlay image from the mask.
    static QImage makeOverlay(const std::vector<bool>& mask, int width, int height);
};

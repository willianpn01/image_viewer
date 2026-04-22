# CImg Viewer

A cross-platform desktop image viewer with editing and AI features, built with **Qt6**, **CImg** and **ONNX Runtime**.

Developed as a practical companion to the book:

> *Digital Image Processing with C++: Implementing Reference Algorithms with the CImg Library*
> — Tschumperlé, Tilmant, Barra (CRC Press, 2023)

---

## Table of Contents

1. [Features](#features)
2. [Technical Architecture](#technical-architecture)
3. [Supported Formats](#supported-formats)
4. [Keyboard Shortcuts](#keyboard-shortcuts)
5. [Minimum Requirements](#minimum-requirements)
6. [Building on Linux](#building-on-linux)
7. [Building for Windows (cross-compilation from Linux)](#building-for-windows-cross-compilation-from-linux)
8. [Running the Application](#running-the-application)
9. [AI Models](#ai-models)
10. [Optional GPU Acceleration (CUDA)](#optional-gpu-acceleration-cuda)
11. [Project Structure](#project-structure)

---

## Features

### File Management
- Open images via file dialog, folder browser, drag-and-drop, or command-line argument
- Save (Ctrl+S) and Save As (Ctrl+Shift+S) in multiple formats
- Recent files menu (last 10 files, persisted across sessions)
- **Batch Export:** process an entire folder — resize, grayscale, brightness, and convert format in one step

### Navigation
- **Folder browser dock** with Quick Access favorites panel (Home, Desktop, Documents, Downloads, Pictures, all Windows drives on Windows)
- Custom favorites: right-click any folder in the tree to add/remove from Quick Access
- Previous/Next image in current folder (Left/Right arrow keys)
- **Slideshow mode** with configurable interval (2 s / 5 s / 10 s / 30 s)

### Viewing
- Zoom in/out with Ctrl+`+`/Ctrl+`-`, mouse wheel, or toolbar
- Fit to window toggle (Ctrl+F)
- Actual size (Ctrl+1)
- Pan with middle-mouse-button drag
- **Before/After compare view** — side-by-side slider comparing original and edited image

### Image Adjustments (with Undo/Redo)
- **Brightness** — slider −100 … +100
- **Contrast** — multiplier 0.1× … 3.0×
- **Gamma correction** — 0.1 … 5.0
- **Grayscale** conversion
- **Negative / invert**
- **Histogram equalization** (CLAHE-style global equalization)

### Filters
- Gaussian blur (adjustable radius, live preview dialog)
- Median blur (adjustable radius, live preview dialog)
- Sharpen / unsharp mask
- Sobel edge detection
- Canny edge detection (adjustable thresholds)
- Emboss
- Non-linear anisotropic diffusion (Perona-Malik, Book Ch. 5)

### Geometric Transforms
- Rotate 90° CW / CCW
- Flip horizontal / vertical
- **Crop** — rubber-band selection on the image, then press Enter or double-click
- **Resize dialog** — width × height with aspect-ratio lock

### Morphological Operations (Book Ch. 4)
- Erosion, Dilation
- Opening, Closing
- Morphological gradient

### Color Tools
- **LAB color transfer** — transfer color palette from a reference image without neural networks

### Annotations
- Draw on the image with configurable color, line width and font size
- Tool palette: freehand pen, straight line, rectangle, ellipse, text
- Annotation undo/redo (separate from image undo/redo)
- Save merged image (annotations burned in) or save annotation data as JSON
- Automatic load/save of `.annotations.json` sidecar files

### AI Tools (ONNX Runtime, optional)
| Feature | Model | Size |
|---|---|---|
| Remove Background | RMBG-1.4 | 168 MB |
| Super Resolution (4×) | Real-ESRGAN | 64 MB |
| Object Detection | YOLOv8n | 12 MB |
| Neural Style Transfer | candy | 6 MB |
| Neural Style Transfer | mosaic | 6 MB |
| Neural Style Transfer | pointilism | 6 MB |
| Neural Style Transfer | rain_princess | 6 MB |

All models are downloaded on demand via the **AI → Manage Models** dialog. No model is bundled with the application.

### Segmentation / Magic Select
- Flood-fill-based magic selection with configurable color threshold (slider)
- Export selected region as a separate image (Ctrl+Shift+E)

### EXIF Panel
- Displays EXIF metadata for JPEG and TIFF images (via TinyEXIF)

### Histogram Panel
- Live RGB and luminance histogram, updated after every edit

### Internationalization
- English and Brazilian Portuguese (pt-BR) interfaces
- Language switch without restart (Settings → Language)

### Logging
- In-app log viewer (Help → View Log)
- Log file persisted to the platform's standard application data directory

---

## Technical Architecture

```
┌─────────────────── Qt6 UI (MainWindow) ──────────────────────┐
│  Menus · Toolbar · Docks · Dialogs · Signals/Slots           │
└──────────────┬───────────────────────────────┬───────────────┘
               │                               │
    ┌──────────▼──────────┐        ┌───────────▼──────────┐
    │   ImageProcessor    │        │      AiTools          │
    │  (CImg algorithms)  │        │  (ONNX Runtime 1.17)  │
    │  brightness/contrast│        │  background removal   │
    │  blur/sharpen/Canny │        │  super-resolution     │
    │  morphology/rotate  │        │  object detection     │
    │  histogram eq       │        │  style transfer       │
    └──────────┬──────────┘        └───────────────────────┘
               │
    ┌──────────▼──────────┐
    │    FileManager      │  Qt load → TGA built-in reader
    │  load / save        │  → CImg generic fallback
    │  recent files       │
    └─────────────────────┘
```

| Layer | Library | Role |
|---|---|---|
| GUI | Qt 6.x (Widgets, Core, Network) | All UI, file dialogs, settings |
| Image I/O | Qt image plugins + FileManager | JPEG, PNG, BMP, GIF, SVG, WebP… |
| Image processing | CImg (header-only) | Pixel algorithms, CImg data type |
| AI inference | ONNX Runtime 1.17.1 | Neural network models |
| EXIF parsing | TinyEXIF | Read EXIF from JPEG/TIFF |
| Build | CMake ≥ 3.16, C++17 | |
| Translations | Qt Linguist (.ts → .qm) | pt-BR / en |

The `CImg<unsigned char>` type is used as the internal image representation. `ImageConvert` handles lossless conversion between `CImg<unsigned char>` and `QImage` for display. `UndoStack` keeps a depth-limited (default 20) history of `CImg` states.

---

## Supported Formats

| Format | Read | Write | Notes |
|---|---|---|---|
| JPEG (.jpg .jpeg) | ✓ | ✓ | via Qt |
| PNG (.png) | ✓ | ✓ | via Qt |
| BMP (.bmp) | ✓ | ✓ | via Qt |
| TIFF (.tiff .tif) | ✓ | ✓ | via Qt |
| WebP (.webp) | ✓* | ✓* | requires Qt WebP plugin |
| GIF (.gif) | ✓* | — | via Qt gif plugin |
| ICO (.ico) | ✓* | — | via Qt ico plugin |
| SVG (.svg .svgz) | ✓* | — | via Qt svg plugin |
| TGA (.tga) | ✓ | ✓ | built-in reader/writer |
| AVIF (.avif) | ✓* | — | Qt 6.5+ with libavif |
| HEIC / HEIF | ✓* | — | OS codec required |
| OpenEXR (.exr) | ✓* | — | via CImg/ImageMagick |
| Netpbm (.pbm .pgm .ppm) | ✓ | — | via CImg |
| Photoshop (.psd) | ✓* | — | via external tools |

\* Plugin/codec must be present on the system.

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| **File** | |
| Ctrl+O | Open file |
| Ctrl+Shift+O | Open folder |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| **Edit** | |
| Ctrl+Z | Undo |
| Ctrl+Y / Ctrl+Shift+Z | Redo |
| **View** | |
| Ctrl++ | Zoom in |
| Ctrl+- | Zoom out |
| Ctrl+F | Fit to window |
| Ctrl+1 | Actual size (100%) |
| Left / Right | Previous / Next image in folder |
| **Crop** | |
| Enter / double-click | Confirm crop selection |
| Escape | Cancel crop |
| **Segmentation** | |
| Ctrl+Shift+E | Export selection |
| **Annotations** | |
| Ctrl+Z / Ctrl+Y | Annotation undo / redo |
| **Help** | |
| F1 | Keyboard shortcuts dialog |

---

## Minimum Requirements

### Linux
| Component | Minimum |
|---|---|
| OS | Ubuntu 22.04 / Debian 12 or equivalent |
| CPU | x86-64, any modern processor |
| RAM | 512 MB (2 GB recommended for AI features) |
| Disk | 200 MB for the build, +270 MB for all AI models |
| Qt | 6.2 or later |
| Compiler | GCC 11+ or Clang 13+ (C++17) |
| CMake | 3.16 or later |

### Windows
| Component | Minimum |
|---|---|
| OS | Windows 10 64-bit (version 1903+) |
| CPU | x86-64 |
| RAM | 512 MB (2 GB recommended for AI features) |
| Disk | 200 MB |
| Runtime | Visual C++ Redistributable 2019+ (included in the zip) |

### GPU (optional)
| Component | Requirement |
|---|---|
| GPU | NVIDIA with CUDA compute capability ≥ 6.0 |
| Driver | ≥ 520 |
| CUDA Toolkit | 11.8 or 12.x |

---

## Building on Linux

### 1. Install dependencies

```bash
# Debian / Ubuntu
sudo apt install -y \
    build-essential cmake git \
    qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools \
    libgl1-mesa-dev

# Fedora / RHEL
sudo dnf install -y \
    gcc-c++ cmake git \
    qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools \
    mesa-libGL-devel
```

### 2. Clone and prepare third-party files

```bash
git clone <repo-url> cimg-viewer
cd cimg-viewer

# CImg (header-only)
curl -L https://github.com/GreycLab/CImg/raw/master/CImg.h \
     -o third_party/CImg.h

# ONNX Runtime CPU (Linux x64)
ORT_VER=1.17.1
curl -L "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VER}/onnxruntime-linux-x64-${ORT_VER}.tgz" \
     -o /tmp/ort.tgz
tar -xzf /tmp/ort.tgz -C third_party/
mv third_party/onnxruntime-linux-x64-${ORT_VER} third_party/onnxruntime
```

### 3. Build

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### 4. Run

```bash
./CImgViewer                   # open empty
./CImgViewer /path/to/photo.jpg
```

---

## Building for Windows (cross-compilation from Linux)

This procedure compiles a native Windows x64 executable on an Ubuntu host using MinGW-w64 and a pre-built Qt6 for Windows.

### 1. Install MinGW and Wine

```bash
sudo apt install -y \
    mingw-w64 wine wine64 \
    cmake ninja-build
```

### 2. Get Qt6 for Windows (MinGW)

Download the Qt 6.7.3 MinGW package from [qt.io/download](https://www.qt.io/download-open-source) using the Qt Online Installer, selecting the **MSVC / MinGW 64-bit** component. Default install path: `~/Qt/6.7.3/mingw_64/`.

Alternatively, use a pre-built mirror:

```bash
# Example using a community mirror (adjust version/path as needed)
QTDIR=~/qt6-windows/6.7.3/mingw_64
```

### 3. Create Wine wrappers for Qt tools

CMake needs to invoke Qt's code-generation tools (`moc`, `uic`, `rcc`) during the build. Because these are `.exe` files, they must be run via Wine — except that Wine remaps `/` to `Z:\`, which corrupts GNU make dependency files.

The solution is to use the **native Linux Qt tools** for code generation (they produce identical output since `Q_MOC_OUTPUT_REVISION = 68` in Qt 6.4–6.7) and Wine only for tools that actually need to run the Windows binaries.

Run the following script to create the wrappers:

```bash
QTBIN=~/qt6-windows/6.7.3/mingw_64/bin
NATIVE=/usr/lib/qt6/libexec   # native Linux Qt libexec

# Rename originals
for f in "$QTBIN"/*.exe; do mv "$f" "${f}.bak"; done

# Native-tool wrappers (no Wine — avoids Z: path contamination)
for tool in moc uic rcc qlalr qvkgen lrelease lupdate lconvert; do
    cat > "$QTBIN/$tool" <<EOF
#!/bin/bash
exec $NATIVE/$tool "\$@"
EOF
    chmod +x "$QTBIN/$tool"
done

# Wine wrappers for the remaining tools
for bak in "$QTBIN"/*.exe.bak; do
    exe="${bak%.bak}"
    name="$(basename "$exe")"
    cat > "${exe%.*}" <<EOF
#!/bin/bash
WINEDEBUG=-all exec wine "$bak" "\$@"
EOF
    chmod +x "${exe%.*}"
done
```

### 4. Get ONNX Runtime for Windows

```bash
ORT_VER=1.17.1
curl -L "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VER}/onnxruntime-win-x64-${ORT_VER}.zip" \
     -o /tmp/ort-win.zip
unzip /tmp/ort-win.zip -d third_party/
mv third_party/onnxruntime-win-x64-${ORT_VER} third_party/onnxruntime-win
```

### 5. Configure and build

```bash
QTDIR=~/qt6-windows/6.7.3/mingw_64

cmake -B build-windows \
    -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
    -DCMAKE_PREFIX_PATH="$QTDIR" \
    -DONNXRUNTIME_DIR="$(pwd)/third_party/onnxruntime-win" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build-windows -j$(nproc)
```

Minimal toolchain file (`cmake/mingw-w64-x86_64.cmake`):

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

### 6. Package

After building, gather all required DLLs next to `CImgViewer.exe`:

```bash
DIST=dist-windows
mkdir -p $DIST/platforms $DIST/styles $DIST/imageformats

cp build-windows/CImgViewer.exe $DIST/

# Qt6 DLLs
for dll in Core Gui Widgets Network Svg SvgWidgets; do
    cp "$QTDIR/bin/Qt6${dll}.dll" $DIST/
done

# MinGW runtime
for lib in libstdc++-6 libgcc_s_seh-1 libwinpthread-1; do
    cp "/usr/x86_64-w64-mingw32/lib/${lib}.dll" $DIST/ 2>/dev/null || \
    find /usr -name "${lib}.dll" -exec cp {} $DIST/ \;
done

# Qt plugins
cp "$QTDIR/plugins/platforms/qwindows.dll"   $DIST/platforms/
cp "$QTDIR/plugins/imageformats/qjpeg.dll"   $DIST/imageformats/
cp "$QTDIR/plugins/imageformats/qgif.dll"    $DIST/imageformats/
cp "$QTDIR/plugins/imageformats/qico.dll"    $DIST/imageformats/
cp "$QTDIR/plugins/imageformats/qsvg.dll"    $DIST/imageformats/

# ONNX Runtime
cp third_party/onnxruntime-win/lib/onnxruntime.dll $DIST/
cp third_party/onnxruntime-win/lib/onnxruntime_providers_shared.dll $DIST/ 2>/dev/null || true

# qt.conf so Qt finds plugins relative to the executable
echo -e "[Paths]\nPlugins = .\nTranslations = translations" > $DIST/qt.conf

# Translations
mkdir -p $DIST/translations
cp translations/*.qm $DIST/translations/ 2>/dev/null || true

# Create ZIP
zip -r CImgViewer-windows-x64.zip $DIST/
```

---

## Running the Application

### Linux

```bash
cd build
./CImgViewer                        # open empty window
./CImgViewer /path/to/image.jpg     # open a specific file
```

The binary uses an embedded RPATH pointing to `third_party/onnxruntime/lib/`, so no `LD_LIBRARY_PATH` is needed when run from the build directory.

### Windows

Unzip `CImgViewer-windows-x64.zip` and run `CImgViewer.exe`. No installation required. All DLLs must remain in the same directory as the executable.

---

## AI Models

Models are **not bundled** with the application. Download them via **AI → Manage Models** or manually place `.onnx` files in the `models/` subdirectory next to the executable (Linux: `build/models/`, Windows: `dist-windows/models/`).

| Model ID | File | Size | Source |
|---|---|---|---|
| `rmbg` | `rmbg-1.4.onnx` | 168 MB | BRIA AI (Apache 2.0) |
| `realesrgan` | `realesrgan-x4.onnx` | 64 MB | xinntao (BSD-3) |
| `yolov8` | `yolov8n.onnx` | 12 MB | Ultralytics (AGPL-3.0) |
| `style_candy` | `style_candy.onnx` | 6 MB | PyTorch examples (BSD-2) |
| `style_mosaic` | `style_mosaic.onnx` | 6 MB | PyTorch examples (BSD-2) |
| `style_pointilism` | `style_pointilism.onnx` | 6 MB | PyTorch examples (BSD-2) |
| `style_rain_princess` | `style_rain_princess.onnx` | 6 MB | PyTorch examples (BSD-2) |

The in-app downloader probes each URL with a HEAD request before downloading, validates file size (≥ 80% of expected), and optionally runs `onnx.checker` via Python 3 if available.

---

## Optional GPU Acceleration (CUDA)

By default the CPU-only ONNX Runtime package is used. To enable CUDA:

**Requirements:** NVIDIA driver ≥ 520, CUDA Toolkit 11.8 or 12.x.

```bash
# Download the GPU-enabled package
ORT_VER=1.17.1
curl -L "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VER}/onnxruntime-linux-x64-gpu-${ORT_VER}.tgz" \
     -o /tmp/ort-gpu.tgz

# Replace the CPU package
rm -rf third_party/onnxruntime
tar -xzf /tmp/ort-gpu.tgz -C third_party/
mv third_party/onnxruntime-linux-x64-gpu-${ORT_VER} third_party/onnxruntime

# Rebuild
cmake --build build -j$(nproc)
```

The CMake `RPATH` automatically picks up whichever `libonnxruntime.so` is in `third_party/onnxruntime/lib/`. No CMake flag change is needed.

---

## Project Structure

```
cimg-viewer/
├── CMakeLists.txt
├── cmake/
│   └── mingw-w64-x86_64.cmake        # Windows cross-compilation toolchain
├── src/
│   ├── main.cpp
│   ├── MainWindow.hpp / .cpp          # Main Qt window, menus, docks
│   ├── ImageProcessor.hpp / .cpp      # CImg-based image algorithms
│   ├── ImageConvert.hpp               # CImg ↔ QImage conversion
│   ├── ImageLabel.hpp / .cpp          # Zoomable, pannable image display widget
│   ├── UndoStack.hpp / .cpp           # Named undo/redo history (CImg states)
│   ├── FileManager.hpp / .cpp         # Open/save/recent, TGA built-in I/O
│   ├── FilterDialog.hpp / .cpp        # Blur/sharpen dialog with live preview
│   ├── HistogramWidget.hpp / .cpp     # RGB/luminance histogram panel
│   ├── CompareView.hpp / .cpp         # Side-by-side before/after slider
│   ├── BatchExportDialog.hpp / .cpp   # Batch folder export dialog
│   ├── ExifPanel.hpp / .cpp           # EXIF metadata dock
│   ├── AnnotationLayer.hpp / .cpp     # Draw-on-image annotation system
│   ├── SegmentTool.hpp / .cpp         # Flood-fill magic selection
│   ├── OnnxEngine.hpp / .cpp          # ONNX Runtime session wrapper
│   ├── ModelManager.hpp / .cpp        # Model download/validation manager
│   ├── ModelManagerDialog.hpp / .cpp  # In-app model download UI
│   ├── AiTools.hpp / .cpp             # AI operation facade (QThread workers)
│   └── Logger.hpp / .cpp             # In-app log system
├── third_party/
│   ├── CImg.h                         # CImg header-only library
│   ├── TinyEXIF.h / .cpp              # Lightweight EXIF reader
│   └── onnxruntime/                   # ONNX Runtime SDK (not included in repo)
│       ├── include/
│       └── lib/
├── translations/
│   ├── cimg-viewer_pt_BR.ts           # Brazilian Portuguese source
│   ├── cimg-viewer_en.ts              # English source
│   ├── translations.qrc               # Qt resource file for .qm files
│   └── *.qm                           # Compiled translation binaries
└── resources/
    ├── app.rc                          # Windows executable icon resource
    └── icons/
```

---

## Credits

- **CImg Library** — David Tschumperlé — [cimg.eu](https://cimg.eu) (CeCILL-C / LGPL)
- **Qt** — The Qt Company — [qt.io](https://www.qt.io) (LGPL 3.0)
- **ONNX Runtime** — Microsoft — [onnxruntime.ai](https://onnxruntime.ai) (MIT)
- **TinyEXIF** — Seacave / tiny-exif contributors (BSD-2-Clause)
- **Book:** *Digital Image Processing with C++* — Tschumperlé, Tilmant, Barra (CRC Press, 2023)

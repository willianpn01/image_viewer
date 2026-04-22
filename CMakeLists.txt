cmake_minimum_required(VERSION 3.16)
project(CImgViewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ImageProcessor.cpp
    src/UndoStack.cpp
    src/FileManager.cpp
    src/HistogramWidget.cpp
    src/ImageLabel.cpp
    src/FilterDialog.cpp
    src/CompareView.cpp
    src/BatchExportDialog.cpp
    src/ExifPanel.cpp
    src/Logger.cpp
    src/SegmentTool.cpp
    src/AnnotationLayer.cpp
    src/OnnxEngine.cpp
    src/ModelManager.cpp
    src/AiTools.cpp
    src/ModelManagerDialog.cpp
    third_party/TinyEXIF.cpp
    translations/translations.qrc
)

# Windows icon resource
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/resources/app.rc")
    list(APPEND SOURCES resources/app.rc)
endif()

# WIN32 flag hides the console window on Windows; no-op on Linux/Mac
if(WIN32)
    add_executable(CImgViewer WIN32 ${SOURCES})
else()
    add_executable(CImgViewer ${SOURCES})
endif()

# Allow overriding ORT location (useful on Windows where paths differ)
if(NOT DEFINED ONNXRUNTIME_DIR)
    set(ONNXRUNTIME_DIR ${CMAKE_SOURCE_DIR}/third_party/onnxruntime)
endif()

# ORT library: .so on Linux/Mac, .lib+.dll on Windows
if(WIN32)
    set(ORT_LIB ${ONNXRUNTIME_DIR}/lib/onnxruntime.lib)
else()
    set(ORT_LIB ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so)
endif()

# Set ONNXRUNTIME_GPU=ON to use the GPU-enabled ORT package (requires replacing
# third_party/onnxruntime with onnxruntime-linux-x64-gpu-1.17.1 — see README).
option(ONNXRUNTIME_GPU "Use GPU-enabled ONNX Runtime (requires CUDA 11.8/12.x)" OFF)

target_include_directories(CImgViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${ONNXRUNTIME_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(CImgViewer PRIVATE
    Qt6::Widgets Qt6::Core Qt6::Network
    ${ORT_LIB}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(CImgViewer PRIVATE pthread)
endif()

# Windows-specific system libraries
if(WIN32)
    target_link_libraries(CImgViewer PRIVATE ws2_32 gdi32)
endif()

# RPATH so the binary finds libonnxruntime.so at runtime (Linux/Mac only)
if(NOT WIN32)
    set_target_properties(CImgViewer PROPERTIES
        BUILD_RPATH "${ONNXRUNTIME_DIR}/lib"
        INSTALL_RPATH "${ONNXRUNTIME_DIR}/lib"
    )
endif()

# Post-build: copy onnxruntime.dll next to the executable on Windows
if(WIN32)
    add_custom_command(TARGET CImgViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:CImgViewer>"
        COMMENT "Copying onnxruntime.dll to build directory"
    )
endif()

# Auto-compile .ts -> .qm using lrelease (qt6-l10n-tools)
find_program(LRELEASE_EXECUTABLE
    NAMES lrelease lrelease-qt6
    PATHS /usr/lib/qt6/bin /usr/bin
    NO_DEFAULT_PATH
)
if(LRELEASE_EXECUTABLE)
    set(TS_FILES
        ${CMAKE_SOURCE_DIR}/translations/cimg-viewer_pt_BR.ts
        ${CMAKE_SOURCE_DIR}/translations/cimg-viewer_en.ts
    )
    foreach(TS_FILE ${TS_FILES})
        get_filename_component(QM_NAME ${TS_FILE} NAME_WE)
        set(QM_FILE ${CMAKE_SOURCE_DIR}/translations/${QM_NAME}.qm)
        add_custom_command(
            OUTPUT ${QM_FILE}
            COMMAND ${LRELEASE_EXECUTABLE} ${TS_FILE} -qm ${QM_FILE}
            DEPENDS ${TS_FILE}
            COMMENT "Compiling ${QM_NAME}.ts -> .qm"
        )
        list(APPEND QM_FILES ${QM_FILE})
    endforeach()
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    add_dependencies(CImgViewer translations)
else()
    message(WARNING "lrelease not found — using pre-built .qm files (install qt6-l10n-tools to auto-compile)")
endif()

target_compile_definitions(CImgViewer PRIVATE
    cimg_display=0
    cimg_verbosity=0
    TINYEXIF_NO_XMP_SUPPORT   # disable tinyxml2 dependency
)

# MinGW cross-compilation: _Frees_ptr_opt_ is used in ONNX Runtime headers but missing from MinGW sal.h
if(MINGW)
    target_compile_definitions(CImgViewer PRIVATE _Frees_ptr_opt_=)
endif()

# Suppress the flood of CImg warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(src/ImageProcessor.cpp PROPERTIES
        COMPILE_FLAGS "-w"
    )
endif()

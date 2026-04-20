cmake_minimum_required(VERSION 3.16)
project(CImgViewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

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
    src/PanHandler.cpp
    third_party/TinyEXIF.cpp
)

add_executable(CImgViewer ${SOURCES})

target_include_directories(CImgViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(CImgViewer PRIVATE Qt6::Widgets)

if(UNIX AND NOT APPLE)
    target_link_libraries(CImgViewer PRIVATE pthread)
endif()

target_compile_definitions(CImgViewer PRIVATE
    cimg_display=0
    cimg_verbosity=0
    TINYEXIF_NO_XMP_SUPPORT   # disable tinyxml2 dependency
)

# Suppress the flood of CImg warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(src/ImageProcessor.cpp PROPERTIES
        COMPILE_FLAGS "-w"
    )
endif()

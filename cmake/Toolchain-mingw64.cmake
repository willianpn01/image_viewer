set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH
    /usr/x86_64-w64-mingw32
    $ENV{HOME}/qt6-windows/6.7.3/mingw_64
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Usar ferramentas Qt nativas do Linux para geração de código
set(QT_HOST_PATH /usr)
set(QT_HOST_PATH_CMAKE_DIR /usr/lib/x86_64-linux-gnu/cmake)
set(CMAKE_CROSSCOMPILING_EMULATOR "")

# Forçar moc/uic/rcc nativos
set(QT_MOC_EXECUTABLE /usr/lib/qt6/libexec/moc)
set(QT_UIC_EXECUTABLE /usr/lib/qt6/libexec/uic)
set(QT_RCC_EXECUTABLE /usr/lib/qt6/libexec/rcc)

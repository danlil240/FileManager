cmake_minimum_required(VERSION 3.22)

project(dolphin-lite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(VTE REQUIRED vte-2.91)

add_executable(dolphin-lite
    resources/app.qrc
    src/main.cpp
    src/app/mainwindow.h
    src/app/mainwindow.cpp
    src/app/filepane.h
    src/app/filepane.cpp
    src/app/fileview.h
    src/app/fileview.cpp
    src/app/breadcrumbbar.h
    src/app/breadcrumbbar.cpp
    src/app/placesmodel.h
    src/app/placesmodel.cpp
    src/app/fileopscontroller.h
    src/app/fileopscontroller.cpp
    src/app/archivecontroller.h
    src/app/archivecontroller.cpp
    src/app/terminalwidget.h
    src/app/terminalwidget.cpp
    src/app/actionlogger.h
    src/app/actionlogger.cpp
    src/app/actionloggerworker.h
    src/app/actionloggerworker.cpp
    src/app/progressdialog.h
    src/app/progressdialog.cpp
)

add_compile_definitions(HAS_VTE)

target_include_directories(dolphin-lite PRIVATE ${VTE_INCLUDE_DIRS})
target_compile_options(dolphin-lite PRIVATE ${VTE_CFLAGS_OTHER})
target_link_libraries(dolphin-lite PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets ${VTE_LIBRARIES})

install(TARGETS dolphin-lite RUNTIME DESTINATION bin)

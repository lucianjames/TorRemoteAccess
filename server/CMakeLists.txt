cmake_minimum_required(VERSION 3.10.0)

project(torRemoteAccess)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE Debug)

#set(CMAKE_CXX_FLAGS "-O3 -s")

find_package(Qt5 CONFIG REQUIRED COMPONENTS Widgets Concurrent)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})


add_executable(torRemoteAccess
   mainwindow.ui
   mainwindow.cpp
   shellwindow.ui
   shellwindow.cpp
   main.cpp
   resources.qrc
)

target_link_libraries(torRemoteAccess
   Qt5::Widgets
   Qt5::Concurrent
   ${OpenCV_LIBS}
)

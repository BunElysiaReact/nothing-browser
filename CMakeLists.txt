cmake_minimum_required(VERSION 3.20)
project(NothingBrowser)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets WebEngineWidgets WebEngineCore Network)

add_executable(nothing-browser
    core/main.cpp
    core/app/MainWindow.cpp
    core/tabs/BrowserTab.cpp
    core/tabs/DevToolsPanel.cpp
    core/tabs/YoutubeTab.cpp
    core/tabs/NewsTab.cpp
    core/engine/Interceptor.cpp
    core/engine/FingerprintSpoofer.cpp
    core/engine/NetworkCapture.cpp
    core/engine/UpdateChecker.cpp
)

target_include_directories(nothing-browser PRIVATE
    core core/app core/tabs core/engine core/bridge)

target_link_libraries(nothing-browser
    Qt6::Core Qt6::Gui Qt6::Widgets
    Qt6::WebEngineWidgets Qt6::WebEngineCore Qt6::Network)
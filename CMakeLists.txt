cmake_minimum_required(VERSION 3.20)
project(NothingBrowser)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
Core Gui Widgets WebEngineWidgets WebEngineCore Network OpenGL)
find_package(OpenSSL REQUIRED)

# ── Shared sources (both binaries use these) ──────────────────────────────────
set(SHARED_SOURCES
    core/engine/PiggyServer.cpp
    core/engine/FingerprintSpoofer.cpp
    core/engine/NetworkCapture.cpp
    core/engine/IdentityGenerator.cpp
    core/engine/IdentityStore.cpp
    core/tabs/PiggyTab.cpp
)

# ── Full browser (headful) ────────────────────────────────────────────────────
add_executable(nothing-browser
    core/main.cpp
    core/app/MainWindow.cpp
    core/tabs/BrowserTab.cpp
    core/tabs/DevToolsPanel.cpp
    core/tabs/YoutubeTab.cpp
    core/tabs/NewsTab.cpp
    core/engine/Interceptor.cpp
    core/engine/UpdateChecker.cpp
    core/engine/PluginManager.cpp
    core/tabs/PluginsTab.cpp
    core/tabs/NewsTab.cpp
    assets/resources.qrc
    ${SHARED_SOURCES}
)

# ── Headless binary (piggy daemon) ───────────────────────────────────────────
add_executable(nothing-browser-headless
    core/main_headless.cpp
    ${SHARED_SOURCES}
)

# ── Link both ─────────────────────────────────────────────────────────────────
set(COMMON_LIBS
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Network
    Qt6::OpenGL
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(nothing-browser PRIVATE
    core core/app core/tabs core/engine core/bridge)

target_include_directories(nothing-browser-headless PRIVATE
    core core/app core/tabs core/engine core/bridge)

target_link_libraries(nothing-browser ${COMMON_LIBS})
target_link_libraries(nothing-browser-headless ${COMMON_LIBS})
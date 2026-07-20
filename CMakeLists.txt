cmake_minimum_required(VERSION 3.20)
project(NothingBrowser)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets WebEngineWidgets WebEngineCore Network OpenGL WebSockets)
find_package(OpenSSL REQUIRED)

# ── Headers that contain Q_OBJECT ────────────────────────────────────────────
set(MOC_HEADERS
    core/app/MainWindow.h
    core/tabs/DevToolsPanel.h
    core/tabs/WelcomeScreen.h
    core/tabs/BrowserTab.h
    core/tabs/ProxyPanel.h
    core/tabs/YoutubeTab.h
    core/tabs/NewsTab.h
    core/tabs/PluginsTab.h
    core/tabs/PiggyTab.h
    core/engine/UpdateChecker.h
    core/engine/NetworkCapture.h
    core/engine/PluginManager.h
    core/engine/CdpProbe.h                    # <-- fixed path
)

# ── PiggyServer — split into logical chunks ───────────────────────────────────
set(PIGGY_SOURCES
    core/engine/piggy/PiggyServer.cpp
    core/engine/piggy/PiggyCommandRouter.cpp
    core/engine/piggy/PiggyTabManager.cpp
    core/engine/piggy/PiggyNavigation.cpp
    core/engine/piggy/PiggyInteractions.cpp
    core/engine/piggy/PiggyCapture.cpp
    core/engine/piggy/PiggyExport.cpp
    core/engine/piggy/PiggyFind.cpp
    core/engine/piggy/PiggyProvide.cpp
    core/engine/piggy/PiggyMedia.cpp
    core/engine/piggy/PiggyProxy.cpp
    core/engine/piggy/PiggyHttp.cpp
    core/engine/piggy/PiggyDialog.cpp
    core/engine/piggy/PiggyPage.cpp
)

# ── Other shared sources ──────────────────────────────────────────────────────
set(SHARED_SOURCES
    core/engine/FingerprintSpoofer.cpp
    core/engine/NetworkCapture.cpp
    core/engine/IdentityGenerator.cpp
    core/engine/IdentityStore.cpp
    core/engine/Interceptor.cpp
    core/engine/ProxyManager.cpp
    core/tabs/PiggyTab.cpp
    ${PIGGY_SOURCES}
    core/engine/piggy/Sessionmanager.cpp
)

# ── DevToolsPanel ─────────────────────────────────────────────────────────────
set(DEVTOOLS_SOURCES
    core/tabs/DevToolsPanel_WelcomeScreen.cpp
    core/tabs/DevToolsPanel_Helpers.cpp
    core/tabs/DevToolsPanel_Constructor.cpp
    core/tabs/DevToolsPanel_DataIngestion.cpp
    core/tabs/DevToolsPanel_RowSelection.cpp
    core/tabs/DevToolsPanel_Actions.cpp
)

# ── Full browser ──────────────────────────────────────────────────────────────
add_executable(nothing-browser
    core/main.cpp
    core/app/MainWindow.cpp
    core/tabs/BrowserTab.cpp
    core/tabs/ProxyPanel.cpp
    core/tabs/YoutubeTab.cpp
    core/tabs/NewsTab.cpp
    core/engine/UpdateChecker.cpp
    core/engine/PluginManager.cpp
    core/tabs/PluginsTab.cpp
    assets/resources.qrc
    ${DEVTOOLS_SOURCES}
    ${SHARED_SOURCES}
    ${MOC_HEADERS}
    core/engine/CdpProbe.cpp                 # <-- fixed path
)

# ── Headless binary ───────────────────────────────────────────────────────────
add_executable(nothing-browser-headless
    core/main_headless.cpp
    ${SHARED_SOURCES}
)

# ── Headful binary ────────────────────────────────────────────────────────────
add_executable(nothing-browser-headful
    core/main_headful.cpp
    ${SHARED_SOURCES}
)

# ── macOS bundle settings ─────────────────────────────────────────────────────
set_target_properties(nothing-browser          PROPERTIES MACOSX_BUNDLE ON)
set_target_properties(nothing-browser-headless PROPERTIES MACOSX_BUNDLE OFF)
set_target_properties(nothing-browser-headful  PROPERTIES MACOSX_BUNDLE OFF)

# ── Common libs ───────────────────────────────────────────────────────────────
set(COMMON_LIBS
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Network
    Qt6::OpenGL
    Qt6::WebSockets
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ── Include dirs ──────────────────────────────────────────────────────────────
set(COMMON_INCLUDES
    core
    core/app
    core/tabs
    core/engine
    core/engine/piggy
    core/bridge
)

target_include_directories(nothing-browser          PRIVATE ${COMMON_INCLUDES})
target_include_directories(nothing-browser-headless PRIVATE ${COMMON_INCLUDES})
target_include_directories(nothing-browser-headful  PRIVATE ${COMMON_INCLUDES})

target_link_libraries(nothing-browser          ${COMMON_LIBS})
target_link_libraries(nothing-browser-headless ${COMMON_LIBS})
target_link_libraries(nothing-browser-headful  ${COMMON_LIBS})
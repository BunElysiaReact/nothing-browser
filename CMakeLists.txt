cmake_minimum_required(VERSION 3.20)
project(NothingBrowser)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets WebEngineWidgets WebEngineCore Network OpenGL)
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
)

# ── PiggyServer — split into logical chunks ───────────────────────────────────
set(PIGGY_SOURCES
    core/engine/piggy/PiggyServer.cpp          # constructor, start/stop, connections
    core/engine/piggy/PiggyCommandRouter.cpp   # handleCommand dispatch
    core/engine/piggy/PiggyTabManager.cpp      # createTab, closeTab, page(), configureProfile
    core/engine/piggy/PiggyNavigation.cpp      # navigate, reload, back/forward, wait, page.*
    core/engine/piggy/PiggyInteractions.cpp    # click, type, hover, keyboard, mouse, evaluate
    core/engine/piggy/PiggyCapture.cpp         # capture start/stop + all capture commands
    core/engine/piggy/PiggyExport.cpp          # session, cookies, intercept, expose, fetch.*
    core/engine/piggy/PiggyMedia.cpp           # screenshot, pdf, image blocking
    core/engine/piggy/PiggyProxy.cpp           # all proxy.* commands + event wiring
    core/engine/piggy/PiggyHttp.cpp
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
    core/engine/piggy/Sessionmanager.cpp   # ← new
)


# ── DevToolsPanel — split into logical chunks ─────────────────────────────────
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
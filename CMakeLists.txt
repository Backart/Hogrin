cmake_minimum_required(VERSION 3.16)
project(Hogrin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Sql)
find_package(Qt6 OPTIONAL_COMPONENTS NetworkInformation)
qt_policy(SET QTP0004 NEW)

# ── libsodium ──────────────────
message(STATUS "Fetching libsodium from source...")
include(FetchContent)
FetchContent_Declare(
    libsodium
    GIT_REPOSITORY https://github.com/robinlinden/libsodium-cmake.git
    GIT_TAG        master
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libsodium)

# ── Sources List ──────────────────────────────
set(HOGRIN_SOURCES
    src/app/main.cpp
    src/network/include/network_manager.h
    src/network/src/network_manager.cpp
    src/network/include/tcp_connection.h
    src/network/src/tcp_connection.cpp
    src/network/include/bootstrap_client.h
    src/network/src/bootstrap_client.cpp
    src/core/include/messenger_core.h
    src/core/src/messenger_core.cpp
    src/ui/backend/ui_handler.h
    src/ui/backend/ui_handler.cpp
    src/common/types.h
    src/common/config.h
    src/crypto/include/crypto_manager.h
    src/crypto/src/crypto_manager.cpp
    src/db/include/local_db.h
    src/db/src/local_db.cpp
    src/updater/update_checker.h
    src/updater/update_checker.cpp
)

qt_add_executable(appHogrin ${HOGRIN_SOURCES})

qt_add_qml_module(appHogrin
    URI Hogrin
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    RESOURCES
        assets/fonts/NotoEmoji-Regular.ttf
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/components/Style.qml
        src/ui/qml/components/MessageDelegate.qml
        src/ui/qml/components/ChatList.qml
        src/ui/qml/components/ChatArea.qml
        src/ui/qml/components/AvatarCircle.qml
        src/ui/qml/components/SectionLabel.qml
        src/ui/qml/components/SettingsCard.qml
        src/ui/qml/components/ModernField.qml
        src/ui/qml/components/AccentButton.qml
        src/ui/qml/components/ModernSwitch.qml
        src/ui/qml/components/AuthScreen.qml
)

# ── Linking ───────────────────────────────────
target_link_libraries(appHogrin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
    Qt6::Qml
    sodium
    $<$<TARGET_EXISTS:Qt6::NetworkInformation>:Qt6::NetworkInformation>
)

target_include_directories(appHogrin PRIVATE
    src/app
    src/core/include
    src/network/include
    src/crypto/include
    src/common
    src/db/include
    src/updater
)

if(TARGET Qt6::NetworkInformation)
    target_compile_definitions(appHogrin PRIVATE HAS_QT_NETWORK_INFORMATION)
endif()

# ── Android ────────────────────────
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
    target_link_libraries(appHogrin PRIVATE Qt6::CorePrivate)
    include(FetchContent)
    FetchContent_Declare(android_openssl GIT_REPOSITORY https://github.com/KDAB/android_openssl.git GIT_TAG master)
    FetchContent_MakeAvailable(android_openssl)
    if(EXISTS "${android_openssl_SOURCE_DIR}/android_openssl.cmake")
        include("${android_openssl_SOURCE_DIR}/android_openssl.cmake")
        add_android_openssl_libraries(appHogrin)
    endif()
    set_target_properties(appHogrin PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_TARGET_SDK_VERSION 34
    )
endif()

# ── Desktop Deployment ────────────────────────
if(NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS appHogrin BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    qt_generate_deploy_qml_app_script(TARGET appHogrin OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${deploy_script})
endif()
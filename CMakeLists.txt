cmake_minimum_required(VERSION 3.16)
project(Hogrin VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Sql)
qt_policy(SET QTP0004 NEW)

# ── libsodium ────────────────────────────────────────────────
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(sodium QUIET libsodium)
endif()

if (sodium_FOUND)
    message(STATUS "Using system libsodium")
else()
    message(STATUS "libsodium not found — fetching from source")
    include(FetchContent)
    FetchContent_Declare(
        libsodium
        GIT_REPOSITORY https://github.com/robinlinden/libsodium-cmake.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(libsodium)
    set(SODIUM_TARGET sodium)
endif()
# ─────────────────────────────────────────────────────────────

if (ANDROID)
    set_target_properties(appHogrin PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
    )
endif()

qt_add_executable(appHogrin
    # App entry point
    src/app/main.cpp
    # Network Layer
    src/network/include/network_manager.h
    src/network/src/network_manager.cpp
    src/network/include/tcp_connection.h
    src/network/src/tcp_connection.cpp
    src/network/include/bootstrap_client.h
    src/network/src/bootstrap_client.cpp
    # Core Logic
    src/core/include/messenger_core.h
    src/core/src/messenger_core.cpp
    # UI Backend
    src/ui/backend/ui_handler.h
    src/ui/backend/ui_handler.cpp
    # Common
    src/common/types.h
    src/common/config.h
    # Crypto
    src/crypto/include/crypto_manager.h
    src/crypto/src/crypto_manager.cpp
    # DB
    src/db/include/db_manager.h
    src/db/src/db_manager.cpp
    src/db/include/local_db.h
    src/db/src/local_db.cpp
)

qt_add_qml_module(appHogrin
    URI Hogrin
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        src/ui/qml/Main.qml
        SOURCES
        QML_FILES src/ui/qml/components/Style.qml
        QML_FILES src/ui/qml/components/MessageDelegate.qml
        QML_FILES src/ui/qml/components/ChatList.qml
        QML_FILES src/ui/qml/components/ChatArea.qml
        QML_FILES src/ui/qml/components/AvatarCircle.qml
        QML_FILES src/ui/qml/components/SectionLabel.qml
        QML_FILES src/ui/qml/components/SettingsCard.qml
        QML_FILES src/ui/qml/components/ModernField.qml
        QML_FILES src/ui/qml/components/AccentButton.qml
        QML_FILES src/ui/qml/components/ModernSwitch.qml
        QML_FILES src/ui/qml/components/ModernSwitch.qml
        QML_FILES src/ui/qml/components/AuthScreen.qml
)

target_link_libraries(appHogrin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
    $<IF:$<BOOL:${sodium_FOUND}>,${sodium_LIBRARIES},${SODIUM_TARGET}>
)

target_include_directories(appHogrin PRIVATE
    src/app
    src/core/include
    src/network/include
    src/crypto/include
    src/common
    src/db/include
    $<$<BOOL:${sodium_FOUND}>:${sodium_INCLUDE_DIRS}>
)

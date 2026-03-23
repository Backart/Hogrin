cmake_minimum_required(VERSION 3.16)
project(Hogrin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Sql)
find_package(Qt6 OPTIONAL_COMPONENTS NetworkInformation)
qt_standard_project_setup(REQUIRES 6.5)

# ── Dependencies ──────────────────────────────────────────────────────────────
include(FetchContent)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND AND NOT ANDROID)
    pkg_check_modules(sodium QUIET libsodium)
endif()

if(NOT sodium_FOUND)
    message(STATUS "libsodium not found — fetching from source")
    FetchContent_Declare(
        libsodium
        GIT_REPOSITORY https://github.com/robinlinden/libsodium-cmake.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(libsodium)
    set(SODIUM_TARGET sodium)
else()
    message(STATUS "Using system libsodium")
    add_library(sodium_interface INTERFACE)
    target_link_libraries(sodium_interface INTERFACE ${sodium_LIBRARIES})
    target_link_directories(sodium_interface INTERFACE ${sodium_LIBRARY_DIRS})
    target_include_directories(sodium_interface INTERFACE ${sodium_INCLUDE_DIRS})
    set(SODIUM_TARGET sodium_interface)
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(CORE_SOURCES
    src/core/include/messenger_core.h
    src/core/src/messenger_core.cpp
    src/crypto/include/crypto_manager.h
    src/crypto/src/crypto_manager.cpp
    src/db/include/local_db.h
    src/db/src/local_db.cpp
)

set(NETWORK_SOURCES
    src/network/include/network_manager.h
    src/network/src/network_manager.cpp
    src/network/include/tcp_connection.h
    src/network/src/tcp_connection.cpp
    src/network/include/bootstrap_client.h
    src/network/src/bootstrap_client.cpp
)

set(UI_SOURCES
    src/ui/backend/ui_handler.h
    src/ui/backend/ui_handler.cpp
    src/updater/update_checker.h
    src/updater/update_checker.cpp
)

qt_add_executable(appHogrin
    src/app/main.cpp
    src/common/types.h
    src/common/config.h
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${UI_SOURCES}
)

# ── QML Module ────────────────────────────────────────────────────────────────
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

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(appHogrin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
    $<$<TARGET_EXISTS:Qt6::NetworkInformation>:Qt6::NetworkInformation>
    ${SODIUM_TARGET}
)

target_include_directories(appHogrin PRIVATE
    src/app
    src/core/include
    src/network/include
    src/crypto/include
    src/common
    src/db/include
    src/updater
    src/ui/backend
)

if(TARGET Qt6::NetworkInformation)
    target_compile_definitions(appHogrin PRIVATE HAS_QT_NETWORK_INFORMATION)
    message(STATUS "Qt6::NetworkInformation found — network change detection enabled")
else()
    message(STATUS "Qt6::NetworkInformation NOT found — network change detection disabled")
endif()

# ── Android ───────────────────────────────────────────────────────────────────
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
    target_link_libraries(appHogrin PRIVATE Qt6::CorePrivate)

    FetchContent_Declare(
        android_openssl
        GIT_REPOSITORY https://github.com/KDAB/android_openssl.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(android_openssl)

    if(EXISTS "${android_openssl_SOURCE_DIR}/android_openssl.cmake")
        include("${android_openssl_SOURCE_DIR}/android_openssl.cmake")
        add_android_openssl_libraries(appHogrin)
    else()
        message(WARNING "android_openssl.cmake not found — OpenSSL will be missing on Android")
    endif()

    set_target_properties(appHogrin PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION    26
        QT_ANDROID_TARGET_SDK_VERSION 34
    )
endif()
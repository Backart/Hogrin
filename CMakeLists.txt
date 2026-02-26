cmake_minimum_required(VERSION 3.16)
project(Hogrin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 1. СНАЧАЛА ИЩЕМ QT
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network)

# 2. ТЕПЕРЬ МОЖНО СТАВИТЬ ПОЛИТИКУ
qt_policy(SET QTP0004 NEW)

# 3. Создаем исполняемый файл приложения
qt_add_executable(appHogrin
    # App entry point
    src/app/main.cpp

    # Network Layer
    src/network/include/network_manager.h
    src/network/src/network_manager.cpp

    src/network/include/tcp_connection.h
    src/network/src/tcp_connection.cpp

    # Core Logic
    src/core/include/messenger_core.h
    src/core/src/messenger_core.cpp

    # UI Backend
    src/ui/backend/ui_handler.h
    src/ui/backend/ui_handler.cpp

    # Common
    src/common/types.h
)

# 4. Настраиваем QML модуль
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
)

# 5. Подключаем библиотеки
target_link_libraries(appHogrin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
)

# 6. Инклуды
target_include_directories(appHogrin PRIVATE
    src/app
    src/core/include
    src/network/include
    src/common
)

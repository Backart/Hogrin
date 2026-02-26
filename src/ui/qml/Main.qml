import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "components"

Window {
    id: root
    visible: true
    width: 980
    height: 720
    minimumWidth: 500
    minimumHeight: 500
    title: "Hogrin Messenger"

    property bool sidebarOpen: false
    property bool isPhone:  width < 600
    property bool isTablet: width >= 600 && width < 1024

    readonly property string currentUsername:
        root.isPhone ? sidebarOverlay.username : sidebar.username

    Style { id: theme }
    ListModel { id: chatModel }

    Connections {
        target: backend
        function onMessage_received(username, text, time) {
            // Пропускаем своё — уже добавили при отправке
            if (username === root.currentUsername) return
            chatModel.append({
                "senderName":  username,
                "messageText": text,
                "isMe":        false,
                "msgTime":     Qt.formatDateTime(time, "hh:mm")
            })
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: root.isPhone ? 0 : 4
            color: SplitHandle.hovered ? theme.accent : theme.border
            Behavior on color { ColorAnimation { duration: 150 } }
            HoverHandler { cursorShape: Qt.SplitHCursor }
            visible: !root.isPhone
        }

        ChatList {
            id: sidebar
            SplitView.preferredWidth: isTablet ? 220 : 280
            SplitView.minimumWidth: 180
            SplitView.maximumWidth: 420
            visible: !root.isPhone
        }

        ChatArea {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
        }
    }

    // Phone sidebar overlay
    ChatList {
        id: sidebarOverlay
        visible: root.isPhone && root.sidebarOpen
        width: Math.min(300, root.width * 0.85)
        height: root.height
        z: 10
    }

    Rectangle {
        visible: root.isPhone && root.sidebarOpen
        x: sidebarOverlay.width
        y: 0
        width: root.width
        height: root.height
        color: "#80000000"
        z: 9
        MouseArea {
            anchors.fill: parent
            onClicked: root.sidebarOpen = false
        }
    }
}

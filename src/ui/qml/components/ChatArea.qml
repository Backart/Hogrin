import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: chatArea
    color: theme.bgMain
    property bool isPeerOnline: false
    property bool isRelayMode: false

    Connections {
        target: backend

        function onPeer_status_changed(nickname, isOnline, isRelay) {
            if (nickname === root.activeChatUser) {
                chatArea.isPeerOnline = isOnline
                chatArea.isRelayMode = isRelay
            }
        }
    }

    Connections {
        target: root
        function onActiveChatUserChanged() {
            chatArea.isPeerOnline = false
        }
    }

    // ── Toast notification ───────────────────────────────────
    property string toastText: ""
    property bool   toastVisible: false
    property color  toastColor: theme.online

    function showToast(msg, isError) {
        toastText = msg
        toastColor = isError ? "#E05C5C" : theme.online
        toastVisible = true
        toastTimer.restart()
    }

    Timer {
        id: toastTimer
        interval: 2800
        onTriggered: chatArea.toastVisible = false
    }

    Rectangle {
        id: toastBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: inputBar.top
        anchors.bottomMargin: chatArea.toastVisible ? 8 : -8
        width: toastLabel.implicitWidth + 32
        height: 36
        radius: theme.radiusFull
        color: chatArea.toastColor
        z: 100
        opacity: chatArea.toastVisible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on anchors.bottomMargin { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: chatArea.toastText
            font.pixelSize: 13
            font.weight: Font.Medium
            color: "#FFFFFF"
        }
    }

    // ── Top bar ─────────────────────────────────────────────
    Rectangle {
        id: topBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 64
        color: theme.bgSide

        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 1; color: theme.border
        }
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.4; color: theme.accent }
                GradientStop { position: 0.7; color: theme.accentGlow }
                GradientStop { position: 1.0; color: "transparent" }
            }
            opacity: 0.6
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 20; rightMargin: 16 }
            spacing: 12

            // ── Hamburger (phone only) — drawn, no symbol ──
            RoundButton {
                visible: root.isPhone
                width: 36; height: 36; flat: true
                contentItem: Column {
                    anchors.centerIn: parent
                    spacing: 4
                    Repeater {
                        model: 3
                        Rectangle {
                            width: 18; height: 2; radius: 1
                            color: theme.textSecondary
                        }
                    }
                }
                background: Rectangle {
                    radius: width / 2
                    color: parent.hovered ? theme.bgHover : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                onClicked: root.sidebarOpen = !root.sidebarOpen
            }

            // ── Chat user info ──
            RowLayout {
                spacing: 12
                visible: root.activeChatUser !== undefined && root.activeChatUser !== ""

                Rectangle {
                    width: 38; height: 38; radius: 12; color: theme.bgActive
                    Rectangle {
                        anchors.fill: parent; radius: parent.radius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#30FFFFFF" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }
                    Text {
                        anchors.centerIn: parent; text: "#"
                        font.pixelSize: 18; font.weight: Font.Bold; color: theme.accent
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: root.activeChatUser !== undefined ? root.activeChatUser : ""
                        font.pixelSize: 15; font.weight: Font.SemiBold; color: theme.text
                    }
                    Row {
                        spacing: 5
                        Rectangle {
                            width: 7; height: 7; radius: 4;
                            color: chatArea.isPeerOnline ? theme.online : theme.textMuted
                            anchors.verticalCenter: parent.verticalCenter
                            SequentialAnimation on opacity {
                                running: chatArea.isPeerOnline
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.4; duration: 1200; easing.type: Easing.InOutSine }
                                NumberAnimation { from: 0.4; to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
                            }
                            opacity: chatArea.isPeerOnline ? 1.0 : 0.5
                        }
                        Text {
                            text: chatArea.isPeerOnline ? "online" : "offline"
                            font.pixelSize: 11; color: theme.textSecondary
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }


            // ── Connection mode badge ──
            Rectangle {
                visible: root.activeChatUser !== undefined && root.activeChatUser !== ""

                Layout.preferredHeight: 22
                Layout.preferredWidth: connModeLabel.implicitWidth + 20

                radius: 11
                color: !chatArea.isPeerOnline ? "transparent" : (chatArea.isRelayMode ? "#2A1A3A" : "#1A3A2A")

                border.color: !chatArea.isPeerOnline ? theme.border : "transparent"
                border.width: 1

                Behavior on color { ColorAnimation { duration: 300 } }
                Behavior on border.color { ColorAnimation { duration: 300 } }

                Text {
                    id: connModeLabel
                    anchors.centerIn: parent
                    text: !chatArea.isPeerOnline ? "---" : (chatArea.isRelayMode ? "Relay e2ee IPv6" : "P2P e2ee IPv6")
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    color: !chatArea.isPeerOnline ? theme.textSecondary : (chatArea.isRelayMode ? "#A07CF5" : "#3DD68C")
                    Behavior on color { ColorAnimation { duration: 300 } }
                }
            }

        }
    }

    // ── Messages ────────────────────────────────────────────
    Item {
        anchors {
            top: topBar.bottom
            left: parent.left
            right: parent.right
            bottom: inputBar.top
        }

        ListView {
            id: messageList
            visible: root.activeChatUser !== undefined && root.activeChatUser !== ""
            anchors.fill: parent
            clip: true
            model: chatModel
            spacing: 2
            topMargin: 16
            bottomMargin: 8

            onCountChanged: Qt.callLater(positionViewAtEnd)

            delegate: MessageDelegate {}

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 3; radius: 2
                    color: theme.textMuted; opacity: 0.6
                }
                background: Item {}
            }
        }

        // внутри Item который содержит ListView, после него:
        Rectangle {
            id: stickyDateBg
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 8
            height: 24
            width: Math.max(stickyDateText.implicitWidth + 20, 60)
            radius: 12
            color: theme.bgSide
            opacity: messageList.moving ? 0.95 : 0
            z: 10
            Behavior on opacity { NumberAnimation { duration: 200 } }

            Text {
                id: stickyDateText
                anchors.centerIn: parent
                font.pixelSize: 11
                font.weight: Font.Medium
                color: theme.textSecondary
                text: {
                    if (chatModel.count === 0) return ""
                    let idx = messageList.indexAt(
                            messageList.contentX + messageList.width / 2,
                            messageList.contentY + 1
                            )
                    if (idx < 0) idx = 0
                    let item = chatModel.get(idx)
                    if (!item || !item.timestamp) return ""
                    let d = new Date(item.timestamp * 1000)
                    let today = new Date()
                    let yesterday = new Date()
                    yesterday.setDate(today.getDate() - 1)
                    if (d.toDateString() === today.toDateString()) return "Today"
                    if (d.toDateString() === yesterday.toDateString()) return "Yesterday"
                    let day   = String(d.getDate()).padStart(2, "0")
                    let month = String(d.getMonth() + 1).padStart(2, "0")
                    let year  = String(d.getFullYear()).slice(-2)
                    return day + "." + month + "." + year
                }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 12
            visible: root.activeChatUser === undefined || root.activeChatUser === ""
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Select a chat to start messaging"
                font.pixelSize: 15; font.weight: Font.SemiBold; color: theme.text
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 12; opacity: 0.5
            visible: root.activeChatUser !== undefined && root.activeChatUser !== "" && chatModel.count === 0
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 64; height: 64; radius: 32
                color: theme.bgActive; border.color: theme.border; border.width: 1
                Text { anchors.centerIn: parent; text: "..."; font.pixelSize: 22; color: theme.textSecondary }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "No messages yet"
                font.pixelSize: 15; font.weight: Font.SemiBold; color: theme.text
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Connect and say hi"
                font.pixelSize: 13; color: theme.textSecondary
            }
        }
    }

    // ── Input bar ───────────────────────────────────────────
    Rectangle {
        id: inputBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 76
        visible: root.activeChatUser !== undefined && root.activeChatUser !== ""
        color: theme.bgSide

        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 1; color: theme.border
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                height: 46; radius: theme.radiusFull
                color: theme.bgInput
                border.color: chatInput.activeFocus ? theme.accent : theme.border
                border.width: chatInput.activeFocus ? 2 : 1

                Rectangle {
                    anchors { fill: parent; margins: -3 }
                    radius: parent.radius + 3; color: "transparent"
                    border.color: theme.accentGlow
                    border.width: chatInput.activeFocus ? 4 : 0
                    opacity: chatInput.activeFocus ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                TextInput {
                    id: chatInput
                    anchors {
                        left: parent.left; right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 18; rightMargin: 18
                    }
                    font.pixelSize: 14; color: theme.text
                    clip: true; selectionColor: theme.accentGlow

                    Text {
                        anchors.fill: parent
                        text: "Write a message..."; font: chatInput.font
                        color: theme.textSecondary
                        visible: chatInput.text.length === 0
                        verticalAlignment: Text.AlignVCenter
                    }
                    Keys.onReturnPressed: sendMessage()
                }
            }

            // ── Send button — drawn arrow, no symbol ──
            Rectangle {
                width: 46; height: 46; radius: width / 2
                color: sendArea.pressed && chatInput.text.length > 0 ? theme.accentHover : theme.accent
                opacity: chatInput.text.length > 0 ? 1.0 : 0.35
                Behavior on opacity { NumberAnimation { duration: 150 } }

                Rectangle {
                    anchors.fill: parent; radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#30FFFFFF" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
                Behavior on color { ColorAnimation { duration: 120 } }

                // Arrow icon — two rectangles
                Text {
                    anchors.centerIn: parent
                    text: ">"
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: "#FFFFFF"
                    leftPadding: 2
                }

                MouseArea {
                    id: sendArea; anchors.fill: parent
                    cursorShape: chatInput.text.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: sendMessage()
                }
            }
        }
    }

    function sendMessage() {
        const txt = chatInput.text.trim()
        if (txt.length === 0) return
        const me   = root.currentUsername
        const peer = root.activeChatUser
        chatModel.append({
                             "senderName":  me,
                             "messageText": txt,
                             "isMe":        true,
                             "msgTime":     Qt.formatDateTime(new Date(), "hh:mm"),
                             "timestamp":   Math.floor(Date.now() / 1000),
                             "is_outgoing": true
                         })
        backend.send_message_from_ui(peer, txt)
        chatInput.text = ""
    }
}
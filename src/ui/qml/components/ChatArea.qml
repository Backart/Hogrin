import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: chatArea
    color: theme.bgMain

    // ── Toast notification ───────────────────────────────────
    property string toastText: ""
    property bool   toastVisible: false
    property color  toastColor: theme.online  // green by default

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
        anchors.bottom: parent.bottom
        anchors.bottomMargin: chatArea.toastVisible ? 88 : 72
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Top bar ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 64
            color: theme.bgSide

            // Subtle bottom border
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1
                color: theme.border
            }

            // Accent glow line at very top
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
                anchors { fill: parent; leftMargin: 20; rightMargin: 20 }
                spacing: 12

                // Hamburger (phone only)
                RoundButton {
                    visible: root.isPhone
                    width: 36; height: 36
                    flat: true
                    contentItem: Text {
                        anchors.centerIn: parent
                        text: "☰"
                        font.pixelSize: 18
                        color: theme.textSecondary
                    }
                    background: Rectangle {
                        radius: width / 2
                        color: parent.hovered ? theme.bgHover : "transparent"
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                    onClicked: root.sidebarOpen = !root.sidebarOpen
                }

                // Channel icon
                Rectangle {
                    width: 38; height: 38
                    radius: 12
                    color: theme.bgActive

                    // Subtle gradient
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#30FFFFFF" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "#"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: theme.accent
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Global Chat"
                        font.pixelSize: 15
                        font.weight: Font.SemiBold
                        color: theme.text
                    }
                    Row {
                        spacing: 5
                        Rectangle {
                            width: 7; height: 7; radius: 4
                            color: theme.online
                            anchors.verticalCenter: parent.verticalCenter

                            // Pulse glow
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.4; duration: 1200; easing.type: Easing.InOutSine }
                                NumberAnimation { from: 0.4; to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
                            }
                        }
                        Text {
                            text: "online"
                            font.pixelSize: 11
                            color: theme.textSecondary
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ── Messages ────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: messageList
                anchors.fill: parent
                clip: true
                model: chatModel
                spacing: 4
                topMargin: 24
                bottomMargin: 16

                onCountChanged: Qt.callLater(positionViewAtEnd)
                delegate: MessageDelegate {}

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 3
                        radius: 2
                        color: theme.textMuted
                        opacity: 0.6
                    }
                    background: Item {}
                }
            }

            // ── Empty state ──────────────────────────────────
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: chatModel.count === 0
                opacity: 0.5

                // Icon circle
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 64; height: 64
                    radius: 32
                    color: theme.bgActive
                    border.color: theme.border
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "💬"
                        font.pixelSize: 26
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No messages yet"
                    font.pixelSize: 15
                    font.weight: Font.SemiBold
                    color: theme.text
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Connect and say hi 👋"
                    font.pixelSize: 13
                    color: theme.textSecondary
                }
            }
        }

        // ── Input bar ───────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 76
            color: theme.bgSide

            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 1
                color: theme.border
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                spacing: 10

                // Input field
                Rectangle {
                    Layout.fillWidth: true
                    height: 46
                    radius: theme.radiusFull
                    color: theme.bgInput
                    border.color: chatInput.activeFocus ? theme.accent : theme.border
                    border.width: chatInput.activeFocus ? 2 : 1

                    // Glow when focused
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -3
                        radius: parent.radius + 3
                        color: "transparent"
                        border.color: theme.accentGlow
                        border.width: chatInput.activeFocus ? 4 : 0
                        opacity: chatInput.activeFocus ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 200 } }
                        Behavior on border.width { NumberAnimation { duration: 200 } }
                    }

                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    TextInput {
                        id: chatInput
                        anchors {
                            left: parent.left; right: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: 18; rightMargin: 18
                        }
                        font.pixelSize: 14
                        color: theme.text
                        clip: true
                        selectionColor: theme.accentGlow

                        Text {
                            anchors.fill: parent
                            text: "Write a message…"
                            font: chatInput.font
                            color: theme.textSecondary
                            visible: chatInput.text.length === 0
                            verticalAlignment: Text.AlignVCenter
                        }

                        Keys.onReturnPressed: sendMessage()
                    }
                }

                // Send button
                Rectangle {
                    width: 46; height: 46
                    radius: width / 2
                    color: sendArea.containsMouse && chatInput.text.length > 0 ? theme.accentHover : theme.accent
                    opacity: chatInput.text.length > 0 ? 1.0 : 0.35
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    // Gradient overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#30FFFFFF" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Behavior on color { ColorAnimation { duration: 120 } }

                    scale: sendArea.containsMouse && chatInput.text.length > 0 ? 1.08 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }

                    Text {
                        anchors.centerIn: parent
                        text: "➤"
                        font.pixelSize: 16
                        color: "#FFFFFF"
                        leftPadding: 2
                    }

                    MouseArea {
                        id: sendArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: chatInput.text.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: sendMessage()
                    }
                }
            }
        }
    }

    function sendMessage() {
        const txt = chatInput.text.trim()
        if (txt.length === 0) return
        const uname = root.currentUsername
        chatModel.append({
            "senderName":  uname,
            "messageText": txt,
            "isMe":        true,
            "msgTime":     Qt.formatDateTime(new Date(), "hh:mm")
        })
        backend.send_message_from_ui(uname, txt)
        chatInput.text = ""
    }
}

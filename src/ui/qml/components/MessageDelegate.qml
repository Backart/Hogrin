import QtQuick
import QtQuick.Layouts

Item {
    id: root
    width: ListView.view ? ListView.view.width : 0
    height: (showDateSep ? dateSep.height : 0) + msgColumn.implicitHeight + 14

    readonly property bool isMe: model.isMe

    signal replyRequested(int msgId, string msgText, string msgSender)
    property bool showReplyMobile: false

    // ── Date separator ────────────────────────────────────────────────────────
    readonly property bool showDateSep: {
        if (model.index === 0) return true
        let prev = ListView.view.model.get(model.index - 1)
        if (!prev) return false
        return new Date(prev.timestamp * 1000).toDateString()
                !== new Date(model.timestamp * 1000).toDateString()
    }

    Item {
        id: dateSep
        visible: showDateSep
        width: parent.width
        height: showDateSep ? 36 : 0
        Row {
            anchors.centerIn: parent; spacing: 8
            Rectangle { width: 40; height: 1; color: theme.border; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: {
                    let d = new Date(model.timestamp * 1000)
                    let today = new Date(); let yesterday = new Date()
                    yesterday.setDate(today.getDate() - 1)
                    if (d.toDateString() === today.toDateString())     return "Today"
                    if (d.toDateString() === yesterday.toDateString()) return "Yesterday"
                    return Qt.formatDate(d, "d MMM yyyy")
                }
                font.pixelSize: 11; font.weight: Font.Medium; color: theme.textSecondary
            }
            Rectangle { width: 40; height: 1; color: theme.border; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    opacity: 0
    Component.onCompleted: fadeIn.start()
    NumberAnimation on opacity { id: fadeIn; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }

    // ── Невидимая зона для Hover и тапов (Вынесена из Row!) ────────────────
    MouseArea {
        id: rowMouseArea
        anchors.fill: msgRow
        hoverEnabled: true
        onClicked: root.showReplyMobile = !root.showReplyMobile
    }

    // ── Message row ───────────────────────────────────────────────────────────
    Row {
        id: msgRow
        anchors {
            left:  isMe ? undefined : parent.left
            right: isMe ? parent.right : undefined
            top:   dateSep.bottom
            leftMargin: 16; rightMargin: 16
        }
        spacing: 8
        layoutDirection: isMe ? Qt.RightToLeft : Qt.LeftToRight

        // ── Avatar ────────────────────────────────────────────────────────────
        Item {
            visible: !isMe; width: 34; height: 34
            Rectangle {
                anchors.fill: parent; radius: width / 2
                color: "transparent"; border.color: avatarCircle.bgColor
                border.width: 1.5; opacity: 0.4
            }
            AvatarCircle {
                id: avatarCircle
                anchors.centerIn: parent; name: model.senderName
                width: 28; height: 28; fontSize: 12
                bgColor: {
                    const colors = ["#7C6EF5","#F5826E","#5BC8AF","#F5D46E","#6EA8F5","#E07CF5"]
                    return colors[(model.senderName ? model.senderName.charCodeAt(0) : 0) % colors.length]
                }
            }
        }

        // ── Bubble column ─────────────────────────────────────────────────────
        Column {
            id: msgColumn
            spacing: 3
            property int maxW: root.width - (isMe ? 72 : 80)

            Text {
                visible: !isMe
                text: model.senderName
                font.pixelSize: 11; font.weight: Font.Medium
                color: theme.textSecondary; leftPadding: 4
            }

            // The bubble
            Rectangle {
                id: bubble
                width: bubbleContent.width + 28
                height: bubbleContent.height + 18
                radius: theme.radius

                // Background
                color: {
                    if (isMe && model.msgStatus === 2) return "#2A0A0A"  // failed — dark red
                    return isMe ? "transparent" : theme.bubbleOther
                }
                border.color: {
                    if (isMe && model.msgStatus === 2) return "#C0392B"   // failed — red border
                    return isMe ? "transparent" : theme.border
                }
                border.width: (isMe && model.msgStatus === 2) ? 1.5 : (isMe ? 0 : 1)

                // Normal outgoing gradient
                Rectangle {
                    visible: isMe && model.msgStatus !== 2
                    anchors.fill: parent; radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.lighter(theme.accent, 1.12) }
                        GradientStop { position: 1.0; color: theme.accent }
                    }
                }

                ColumnLayout {
                    id: bubbleContent
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 14
                    anchors.topMargin: 9

                    width: Math.min(implicitWidth, msgColumn.maxW - 28)
                    spacing: 6

                    // --- Reply quote block ---
                    Rectangle {
                        visible: model.replyToId >= 0 && model.replyToText !== ""
                        Layout.fillWidth: true
                        Layout.preferredHeight: replyContent.implicitHeight + 12
                        radius: 6
                        color: isMe ? "#20FFFFFF" : "#20000000"
                        border.color: isMe ? "#60FFFFFF" : theme.accent
                        border.width: 1

                        Rectangle {
                            width: 3; height: parent.height; radius: 2
                            anchors.left: parent.left
                            color: isMe ? "#FFFFFFC0" : theme.accent
                        }

                        ColumnLayout {
                            id: replyContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: model.replyToSenderName ? model.replyToSenderName : "Unknown"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: isMe ? "#FFFFFF" : theme.accent
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.replyToText || ""
                                font.pixelSize: 11
                                color: isMe ? "#FFFFFFB0" : theme.textSecondary
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }
                    }

                    // --- Message text + time + status row ---
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            Layout.fillWidth: true
                            text: model.messageText
                            color: isMe ? theme.bubbleMeText : theme.bubbleOtherText
                            wrapMode: Text.Wrap
                            font.pixelSize: 14; lineHeight: 1.35
                        }

                        // Time + status indicator
                        Row {
                            Layout.alignment: Qt.AlignBottom | Qt.AlignRight
                            spacing: 3

                            Text {
                                text: model.msgTime
                                font.pixelSize: 10
                                color: (isMe && model.msgStatus === 2) ? "#FF6B6B" : (isMe ? "#FFFFFFB0" : theme.textSecondary)
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // Status icon — outgoing messages only
                            Text {
                                visible: isMe
                                anchors.verticalCenter: parent.verticalCenter
                                font.pixelSize: 11
                                text: {
                                    if (model.msgStatus === 0) return "○"   // pending
                                    if (model.msgStatus === 1) return "✓"   // sent
                                    if (model.msgStatus === 2) return "✕"   // failed
                                    if (model.msgStatus === 3) return "✓✓"  // read
                                    return ""
                                }
                                color: {
                                    if (model.msgStatus === 0) return "#FFFFFF60"
                                    if (model.msgStatus === 1) return "#FFFFFFB0"
                                    if (model.msgStatus === 2) return "#FF6B6B"
                                    if (model.msgStatus === 3) return "#3DD68C"  // green on read
                                    return "#FFFFFFB0"
                                }
                            }
                        }
                    }
                }
            }

            // "Failed to send" tap-to-retry hint
            Text {
                visible: isMe && model.msgStatus === 2
                text: "Not delivered — tap to retry"
                font.pixelSize: 10
                color: "#FF6B6B"
                leftPadding: 4
            }
        }

        // ── Reply button (hover/press) ─────────────────────────────────
        Rectangle {
            id: replyBtn
            visible: rowMouseArea.containsMouse || root.showReplyMobile
            width: 28; height: 28; radius: 14
            color: theme.bgHover
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: "↩"; font.pixelSize: 14
                color: theme.textSecondary
            }
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.showReplyMobile = false
                    root.replyRequested(model.msgId, model.messageText, model.senderName)
                }
            }
        }
    }
}
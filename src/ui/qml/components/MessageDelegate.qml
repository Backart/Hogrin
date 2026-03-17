import QtQuick
import QtQuick.Layouts

Item {
    id: root
    width: ListView.view ? ListView.view.width : 0
    height: (showDateSep ? dateSep.height : 0) + row.implicitHeight + 14

    readonly property bool isMe: model.isMe

    // ── Date separator logic ────────────────────────────────
    readonly property bool showDateSep: {
        if (model.index === 0) return true
        let prev = ListView.view.model.get(model.index - 1)
        if (!prev) return false
        let prevDate = new Date(prev.timestamp * 1000)
        let curDate  = new Date(model.timestamp * 1000)
        return prevDate.toDateString() !== curDate.toDateString()
    }

    Item {
        id: dateSep
        visible: showDateSep
        width: parent.width
        height: showDateSep ? 36 : 0
        Row {
            anchors.centerIn: parent
            spacing: 8
            Rectangle { width: 40; height: 1; color: theme.border; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: {
                    let d = new Date(model.timestamp * 1000)
                    let today     = new Date()
                    let yesterday = new Date()
                    yesterday.setDate(today.getDate() - 1)
                    if (d.toDateString() === today.toDateString())     return "Today"
                    if (d.toDateString() === yesterday.toDateString()) return "Yesterday"
                    return Qt.formatDate(d, "d MMM yyyy")
                }
                font.pixelSize: 11
                font.weight: Font.Medium
                color: theme.textSecondary
            }
            Rectangle { width: 40; height: 1; color: theme.border; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    opacity: 0
    Component.onCompleted: fadeIn.start()
    NumberAnimation on opacity {
        id: fadeIn
        from: 0; to: 1; duration: 200
        easing.type: Easing.OutCubic
    }

    Row {
        id: row
        anchors {
            left:  isMe ? undefined : parent.left
            right: isMe ? parent.right : undefined
            top:   dateSep.bottom
            leftMargin: 16; rightMargin: 16
        }
        spacing: 8
        layoutDirection: isMe ? Qt.RightToLeft : Qt.LeftToRight

        Item {
            visible: !isMe
            width: 34; height: 34
            Rectangle {
                anchors.fill: parent; radius: width / 2
                color: "transparent"; border.color: avatarCircle.bgColor
                border.width: 1.5; opacity: 0.4
            }
            AvatarCircle {
                id: avatarCircle
                anchors.centerIn: parent
                name: model.senderName
                width: 28; height: 28; fontSize: 12
                bgColor: {
                    const colors = ["#7C6EF5","#F5826E","#5BC8AF","#F5D46E","#6EA8F5","#E07CF5"]
                    const i = (model.senderName ? model.senderName.charCodeAt(0) : 0) % colors.length
                    return colors[i]
                }
            }
        }

        Column {
            spacing: 3
            property int maxW: root.width - (isMe ? 72 : 80)

            Text {
                visible: !isMe
                text: model.senderName
                font.pixelSize: 11; font.weight: Font.Medium
                color: theme.textSecondary; leftPadding: 4
            }

            Rectangle {
                width:  Math.min(bubbleContent.implicitWidth + 28, parent.maxW)
                height: bubbleContent.implicitHeight + 18
                radius: theme.radius
                color: isMe ? "transparent" : theme.bubbleOther
                border.color: isMe ? "transparent" : theme.border
                border.width: isMe ? 0 : 1

                Rectangle {
                    visible: isMe
                    anchors.fill: parent; radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.lighter(theme.accent, 1.12) }
                        GradientStop { position: 1.0; color: theme.accent }
                    }
                }

                RowLayout {
                    id: bubbleContent
                    anchors {
                        fill: parent
                        leftMargin: 14; rightMargin: 14
                        topMargin: 9; bottomMargin: 9
                    }
                    spacing: 10
                    Text {
                        text: model.messageText
                        color: isMe ? theme.bubbleMeText : theme.bubbleOtherText
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        font.pixelSize: 14; lineHeight: 1.35
                    }
                    Text {
                        text: model.msgTime
                        font.pixelSize: 10
                        color: isMe ? "#FFFFFFB0" : theme.textSecondary
                        Layout.alignment: Qt.AlignBottom
                    }
                }
            }
        }
    }
}
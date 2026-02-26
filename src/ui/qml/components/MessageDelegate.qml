import QtQuick
import QtQuick.Layouts

Item {
    id: root
    width: ListView.view ? ListView.view.width : 0
    height: row.implicitHeight + 14

    readonly property bool isMe: model.isMe

    opacity: 0
    Component.onCompleted: fadeIn.start()

    NumberAnimation on opacity {
        id: fadeIn
        from: 0; to: 1
        duration: 200
        easing.type: Easing.OutCubic
    }

    Row {
        id: row
        anchors {
            left:  isMe ? undefined : parent.left
            right: isMe ? parent.right : undefined
            leftMargin:  16
            rightMargin: 16
            verticalCenter: parent.verticalCenter
        }
        spacing: 8
        layoutDirection: isMe ? Qt.RightToLeft : Qt.LeftToRight

        // Avatar (only for others)
        Item {
            visible: !isMe
            width: 34; height: 34

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.color: avatarCircle.bgColor
                border.width: 1.5
                opacity: 0.4
            }

            AvatarCircle {
                id: avatarCircle
                anchors.centerIn: parent
                name: model.senderName
                width: 28; height: 28
                fontSize: 12
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

            // Sender name (only for others)
            Text {
                visible: !isMe
                text: model.senderName
                font.pixelSize: 11
                font.weight: Font.Medium
                color: theme.textSecondary
                leftPadding: 4
            }

            // Bubble
            Rectangle {
                width:  Math.min(bubbleContent.implicitWidth + 28, parent.maxW)
                height: bubbleContent.implicitHeight + 18
                radius: theme.radius
                color: isMe ? "transparent" : theme.bubbleOther
                border.color: isMe ? "transparent" : theme.border
                border.width: isMe ? 0 : 1

                // Gradient fill for "me" bubbles
                Rectangle {
                    visible: isMe
                    anchors.fill: parent
                    radius: parent.radius
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
                        font.pixelSize: 14
                        lineHeight: 1.35
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

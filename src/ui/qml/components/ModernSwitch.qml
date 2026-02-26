import QtQuick

Item {
    id: toggle
    implicitWidth: 44
    implicitHeight: 24

    property bool checked: false
    signal toggled()

    Rectangle {
        id: track
        width: 44; height: 24
        radius: 12
        color: toggle.checked ? theme.accent : theme.bgHover
        border.color: toggle.checked ? theme.accent : theme.border

        Behavior on color { ColorAnimation { duration: 160 } }

        Rectangle {
            id: thumb
            width: 18; height: 18
            radius: 9
            color: "#FFFFFF"
            x: toggle.checked ? parent.width - width - 3 : 3
            y: (parent.height - height) / 2

            Behavior on x {
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            toggle.checked = !toggle.checked
            toggle.toggled()
        }
    }
}

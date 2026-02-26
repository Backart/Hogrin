import QtQuick

Item {
    id: root
    width: 40; height: 40

    property string name:     "?"
    property color  bgColor:  theme.accent
    property int    fontSize: 15
    property bool   showOnline: false

    // Subtle glow ring behind avatar
    Rectangle {
        anchors.centerIn: parent
        width:  parent.width + 6
        height: parent.height + 6
        radius: width / 2
        color: "transparent"
        border.color: root.bgColor
        border.width: 1.5
        opacity: 0.3
    }

    // Main avatar circle
    Rectangle {
        id: circle
        anchors.fill: parent
        radius: width / 2
        color: root.bgColor

        // Subtle top-light gradient overlay
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#30FFFFFF" }
                GradientStop { position: 1.0; color: "#00FFFFFF" }
            }
        }

        Text {
            anchors.centerIn: parent
            text: root.name.length > 0 ? root.name[0].toUpperCase() : "?"
            font.pixelSize: root.fontSize
            font.weight: Font.Bold
            color: "#FFFFFF"
        }
    }

    // Online indicator dot
    Rectangle {
        visible: root.showOnline
        width: 10; height: 10
        radius: width / 2
        color: theme.online
        anchors {
            right: parent.right
            bottom: parent.bottom
            rightMargin: -1
            bottomMargin: -1
        }
        // White ring around dot
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 3
            height: parent.height + 3
            radius: width / 2
            color: "transparent"
            border.color: theme.bgSide
            border.width: 1.5
            z: -1
        }
    }
}

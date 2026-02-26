import QtQuick
import QtQuick.Controls

TextField {
    id: control
    height: 40
    font.pixelSize: 14
    color: theme.text
    placeholderTextColor: theme.textSecondary
    leftPadding: 12; rightPadding: 12

    background: Rectangle {
        radius: theme.radiusSmall
        color: theme.bgMain
        border.color: control.activeFocus ? theme.accent : theme.border
        border.width: control.activeFocus ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: 150 } }
        Behavior on border.width { NumberAnimation { duration: 150 } }
    }
}

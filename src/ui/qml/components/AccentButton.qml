import QtQuick
import QtQuick.Controls

Button {
    id: btn
    height: 38
    property bool flat_style: false
    font.pixelSize: 14
    font.weight: Font.Medium

    contentItem: Text {
        text: btn.text
        font: btn.font
        color: btn.flat_style
               ? (btn.hovered ? theme.accent : theme.textSecondary)
               : theme.accentText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: theme.radiusSmall
        color: {
            if (btn.flat_style) return btn.hovered ? theme.bgHover : theme.bgInput
            return btn.hovered ? theme.accentHover : theme.accent
        }
        border.color: btn.flat_style ? theme.border : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}

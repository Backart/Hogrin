import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card
    default property alias content: inner.children
    implicitHeight: inner.height + 28
    color: theme.bgInput
    radius: theme.radiusSmall
    border.color: theme.border
    clip: true

    Item {
        id: inner
        anchors {
            left: parent.left; right: parent.right; top: parent.top
            leftMargin: 14; rightMargin: 14; topMargin: 14
        }
        height: childrenRect.height
    }
}

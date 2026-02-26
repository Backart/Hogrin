import QtQuick

Text {
    property string label: ""
    text: label.toUpperCase()
    font.pixelSize: 11
    font.weight: Font.Bold
    font.letterSpacing: 1.2
    color: theme.textSecondary
}

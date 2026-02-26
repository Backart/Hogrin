import QtQuick

QtObject {
    id: styleObj
    property bool isDarkMode: true

    // Backgrounds — slightly warmer, more depth
    property color bgMain:    isDarkMode ? "#0D0F18" : "#F5F6FA"
    property color bgSide:    isDarkMode ? "#13151F" : "#FFFFFF"
    property color bgInput:   isDarkMode ? "#1A1C28" : "#EDEEF5"
    property color bgHover:   isDarkMode ? "#1E2030" : "#E8EAF2"
    property color bgActive:  isDarkMode ? "#232640" : "#E0E3F5"

    // Accent — a touch more violet, pops better
    readonly property color accent:      "#6B7FFF"
    readonly property color accentHover: "#5A6EEE"
    readonly property color accentText:  "#FFFFFF"
    readonly property color accentGlow:  isDarkMode ? "#6B7FFF40" : "#6B7FFF25"

    // Text
    property color text:          isDarkMode ? "#ECEEF6" : "#1A1C2E"
    property color textSecondary: isDarkMode ? "#636880" : "#8A90A8"
    property color textMuted:     isDarkMode ? "#3C3F55" : "#C4C8D8"

    // Borders
    property color border:  isDarkMode ? "#1F2235" : "#E4E6F2"
    property color divider: isDarkMode ? "#181A28" : "#EEF0F8"

    // Bubbles
    readonly property color bubbleMe:        accent
    readonly property color bubbleMeText:    "#FFFFFF"
    property color bubbleOther:     isDarkMode ? "#1A1C2C" : "#FFFFFF"
    property color bubbleOtherText: isDarkMode ? "#ECEEF6" : "#1A1C2E"

    // Status
    readonly property color online:  "#3DD68C"
    readonly property color offline: "#636880"

    // Radii
    readonly property int radius:      18
    readonly property int radiusSmall: 10
    readonly property int radiusFull:  999

    // Elevation / shadow (use as drop shadow color)
    readonly property color shadowColor: isDarkMode ? "#00000060" : "#00000018"
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: sidebar
    color: theme.bgSide

    property alias username: myUsernameField.text
    property bool  showSettings: false

    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: if (root.isPhone) root.sidebarOpen = false
    }

    Rectangle {
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
        width: 1
        color: theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header ──
        Rectangle {
            Layout.fillWidth: true
            height: 64
            color: "transparent"

            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
                spacing: 10

                AvatarCircle {
                    name: sidebar.username
                    bgColor: theme.accent
                    width: 36; height: 36
                    fontSize: 14
                }

                Text {
                    Layout.fillWidth: true
                    text: sidebar.username.length > 0 ? sidebar.username : "You"
                    font.pixelSize: 15
                    font.weight: Font.SemiBold
                    color: theme.text
                    elide: Text.ElideRight
                }

                RoundButton {
                    id: settingsBtn
                    width: 36; height: 36
                    flat: true
                    checkable: true
                    checked: showSettings
                    onCheckedChanged: showSettings = checked

                    contentItem: Text {
                        anchors.centerIn: parent
                        text: settingsBtn.checked ? "✕" : "⚙"
                        font.pixelSize: 16
                        color: settingsBtn.checked ? theme.accent : theme.textSecondary
                    }
                    background: Rectangle {
                        radius: width / 2
                        color: settingsBtn.hovered ? theme.bgHover : "transparent"
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: showSettings ? 1 : 0

            // ── Chats tab ──
            Item {
                clip: true
                ListView {
                    anchors.fill: parent
                    model: ["Global Chat"]
                    topMargin: 8; bottomMargin: 8

                    delegate: ItemDelegate {
                        width: parent.width
                        height: 60
                        highlighted: true

                        background: Rectangle {
                            radius: theme.radiusSmall
                            color: theme.bgActive
                            Rectangle {
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                                width: 3; radius: 2
                                color: theme.accent
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 10

                            AvatarCircle {
                                name: "#"
                                fontSize: 16
                                width: 38; height: 38
                                Rectangle {
                                    anchors.fill: parent
                                    radius: width / 2
                                    color: "#2B3080"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "#"
                                        font.pixelSize: 16
                                        font.weight: Font.Bold
                                        color: theme.accent
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    text: modelData
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.text
                                }
                                Text {
                                    text: chatModel.count + " messages"
                                    font.pixelSize: 12
                                    color: theme.textSecondary
                                }
                            }
                        }
                    }
                }
            }

            // ── Settings tab ──
            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: sidebar.width - 24
                    x: 12
                    spacing: 0

                    Item { height: 16 }

                    SectionLabel { label: "Appearance" }
                    Item { height: 8 }

                    // Dark mode — без SettingsCard, простой Rectangle
                    Rectangle {
                        Layout.fillWidth: true
                        height: 52
                        color: theme.bgInput
                        radius: theme.radiusSmall
                        border.color: theme.border

                        Text {
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 14 }
                            text: "Dark Mode"
                            font.pixelSize: 14
                            color: theme.text
                        }

                        ModernSwitch {
                            id: dmSwitch
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 14 }
                            Component.onCompleted: checked = theme.isDarkMode
                            onToggled: theme.isDarkMode = checked
                        }
                    }

                    Item { height: 20 }

                    SectionLabel { label: "Identity" }
                    Item { height: 8 }

                    SettingsCard {
                        Layout.fillWidth: true
                        ModernField {
                            id: myUsernameField
                            width: parent.width
                            placeholderText: "Username"
                            text: root.loggedInUser
                            readOnly: true
                        }
                    }

                    Item { height: 20 }

                    SectionLabel { label: "Connection" }
                    Item { height: 8 }

                    SettingsCard {
                        Layout.fillWidth: true

                        ColumnLayout {
                            width: parent.width
                            spacing: 10

                            ModernField {
                                id: ipField
                                Layout.fillWidth: true
                                placeholderText: "Host IP"
                                text: "127.0.0.1"
                            }
                            ModernField {
                                id: portField
                                Layout.fillWidth: true
                                placeholderText: "Port"
                                text: "1234"
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                AccentButton {
                                    text: "Host"
                                    Layout.fillWidth: true
                                    flat_style: true
                                    onClicked: backend.start_server(parseInt(portField.text))
                                }
                                AccentButton {
                                    text: "Join"
                                    Layout.fillWidth: true
                                    onClicked: backend.connect_to_host(ipField.text, parseInt(portField.text))
                                }
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }
        }
    }
}

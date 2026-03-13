import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: sidebar
    color: theme.bgSide

    property alias username: myUsernameField.text
    property bool  showSettings: false

    function loadSavedContacts() {
        contactsModel.clear()
        let peers = backend.get_recent_chats()

        for (let i = 0; i < peers.length; ++i) {
            contactsModel.append({
                                     "nickname": peers[i],
                                     "isOnline": false
                                 })
        }
    }

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

                ListModel { id: contactsModel }

                Connections {
                    target: backend

                    function onPeer_found(host, port) {
                        let foundName = searchField.text.trim() || root.activeChatUser

                        let exists = false;
                        for (let i = 0; i < contactsModel.count; ++i) {
                            if (contactsModel.get(i).nickname === foundName) {
                                contactsModel.setProperty(i, "isOnline", true)
                                exists = true;
                                break;
                            }
                        }

                        if (!exists) {
                            contactsModel.append({
                                                     "nickname": foundName,
                                                     "isOnline": true
                                                 })
                        }

                        root.activeChatUser = foundName
                        searchToast.statusText = ""
                    }

                    function onPeer_not_found() {
                        contactsModel.clear()
                        searchToast.searchSuccess = false
                        searchToast.statusText = "Not found"
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Search bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 52
                        color: "transparent"

                        RowLayout {
                            anchors { fill: parent; leftMargin: 12; rightMargin: 12 }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 36
                                radius: theme.radiusFull
                                color: theme.bgInput
                                border.color: searchField.activeFocus ? theme.accent : theme.border
                                border.width: searchField.activeFocus ? 2 : 1

                                RowLayout {
                                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                                    spacing: 6
                                    Text {
                                        text: "@"
                                        font.pixelSize: 14
                                        font.weight: Font.Bold
                                        color: theme.accent
                                    }
                                    TextInput {
                                        id: searchField
                                        Layout.fillWidth: true
                                        font.pixelSize: 13
                                        color: theme.text
                                        clip: true

                                        Timer {
                                            id: searchDebounce
                                            interval: 500
                                            repeat: false
                                            onTriggered: {
                                                let t = searchField.text.trim()
                                                if (t.length >= 3) {
                                                    searchToast.searchSuccess = false
                                                    searchToast.statusText = "Searching..."
                                                    backend.find_peer(t)
                                                }
                                            }
                                        }

                                        onTextChanged: {
                                            let t = text.trim()
                                            if (t.length === 0) {
                                                searchDebounce.stop()
                                                loadSavedContacts()
                                                searchToast.statusText = ""
                                            } else {
                                                searchDebounce.restart()
                                            }
                                        }

                                        Text {
                                            anchors.fill: parent
                                            text: "find by nickname..."
                                            font: searchField.font
                                            color: theme.textSecondary
                                            visible: searchField.text.length === 0
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        Keys.onReturnPressed: {
                                            let t = searchField.text.trim()
                                            if (t.length > 0) {
                                                searchDebounce.stop()
                                                searchToast.searchSuccess = false
                                                searchToast.statusText = "Searching..."
                                                backend.find_peer(t)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.border }

                    // Toast
                    Rectangle {
                        id: searchToast
                        Layout.fillWidth: true
                        height: statusText.length > 0 ? 40 : 0
                        color: searchSuccess ? "#1A3A1A" : "#3A2A2A"
                        clip: true

                        property bool searchSuccess: false
                        property string statusText: ""

                        Behavior on height { NumberAnimation { duration: 200 } }

                        Text {
                            anchors.centerIn: parent
                            text: searchToast.statusText
                            font.pixelSize: 13
                            color: searchToast.searchSuccess ? theme.online : "#E59A5A"
                        }
                    }

                    // Contacts list
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: contactsModel
                        topMargin: 8; bottomMargin: 8
                        clip: true

                        Item {
                            anchors.centerIn: parent
                            visible: contactsModel.count === 0 && searchField.text.length === 0
                            Column {
                                anchors.centerIn: parent
                                spacing: 8
                                opacity: 0.4
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "🔍"
                                    font.pixelSize: 28
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Search contacts"
                                    font.pixelSize: 13
                                    color: theme.textSecondary
                                }
                            }
                        }

                        delegate: ItemDelegate {
                            width: ListView.view ? ListView.view.width : 0
                            height: 60

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
                                    name: model.nickname
                                    fontSize: 14
                                    width: 38; height: 38
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        text: model.nickname
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                        color: theme.text
                                    }
                                    Row {
                                        spacing: 5
                                        Rectangle {
                                            width: 7; height: 7; radius: 4
                                            color: model.isOnline ? theme.online : theme.textMuted
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Text {
                                            text: model.isOnline ? "online" : "offline"
                                            font.pixelSize: 11
                                            color: theme.textSecondary
                                        }
                                    }
                                }
                            }

                            onClicked: {
                                root.activeChatUser = model.nickname
                                backend.find_peer(model.nickname)
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

                    SectionLabel { label: "Account" }
                    Item { height: 8 }

                    AccentButton {
                        Layout.fillWidth: true
                        text: "Sign Out"
                        flat_style: true
                        onClicked: {
                            backend.logout()
                            root.isAuthenticated = false
                            root.loggedInUser = ""

                            root.activeChatUser = ""
                            contactsModel.clear()
                            if (typeof chatModel !== "undefined") {
                                chatModel.clear()
                            }
                        }
                    }

                    Item { height: 20 }
                }
            }
        }
    }
}

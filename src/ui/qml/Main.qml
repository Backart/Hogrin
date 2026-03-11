import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "components"

Window {
    id: root
    visible: true
    width: 980
    height: 720
    minimumWidth: 500
    minimumHeight: 500
    title: "Hogrin Messenger"

    property bool sidebarOpen: false
    property bool isPhone:  width < 600
    property bool isTablet: width >= 600 && width < 1024
    property bool isAuthenticated: false
    property string loggedInUser: ""
    property string activeChatUser: ""

    readonly property string currentUsername: loggedInUser

    Style { id: theme }
    ListModel { id: chatModel }

    onActiveChatUserChanged: {
        chatModel.clear()

        if (activeChatUser === "") return;

        let history = backend.load_history(activeChatUser, 100);

        for (let i = 0; i < history.length; ++i) {
            let msg = history[i];
            let date = new Date(msg.timestamp * 1000);

            chatModel.append({
                                 "senderName":  msg.sender,
                                 "messageText": msg.text,
                                 "isMe":        Boolean(msg.is_outgoing),
                                 "msgTime":     Qt.formatDateTime(date, "hh:mm")
                             });
        }
    }

    Component.onCompleted: {
        backend.check_saved_session()
    }

    Connections {
        target: backend
        function onSession_restored(nickname) {
            if (nickname.length === 0) return

            root.loggedInUser = nickname
            root.isAuthenticated = true
            backend.start_server(0)
            backend.register_on_bootstrap(nickname)
            sidebar.loadSavedContacts()
            sidebarOverlay.loadSavedContacts()
        }
    }

    Connections {
        target: backend
        function onMessage_received(username, text, time) {
            if (username === root.currentUsername) return

            if (username === root.activeChatUser) {
                chatModel.append({
                                     "senderName":  username,
                                     "messageText": text,
                                     "isMe":        false,
                                     "msgTime":     Qt.formatDateTime(time, "hh:mm")
                                 })
            }
        }
    }

    // ── Auth Screen ──────────────────────────────────────────
    AuthScreen {
        id: authScreen
        anchors.fill: parent
        visible: !root.isAuthenticated
        z: 100

        onAuthSuccess: function(username) {
            root.loggedInUser = username
            root.isAuthenticated = true
            backend.register_on_bootstrap(username)
            sidebar.loadSavedContacts()
            sidebarOverlay.loadSavedContacts()
        }
    }

    // ── Main UI ──────────────────────────────────────────────
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal
        visible: root.isAuthenticated

        handle: Rectangle {
            implicitWidth: root.isPhone ? 0 : 4
            color: SplitHandle.hovered ? theme.accent : theme.border
            Behavior on color { ColorAnimation { duration: 150 } }
            HoverHandler { cursorShape: Qt.SplitHCursor }
            visible: !root.isPhone
        }

        ChatList {
            id: sidebar
            SplitView.preferredWidth: isTablet ? 220 : 280
            SplitView.minimumWidth: 180
            SplitView.maximumWidth: 420
            visible: !root.isPhone
        }

        ChatArea {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
        }
    }

    // Phone sidebar overlay
    ChatList {
        id: sidebarOverlay
        visible: root.isPhone && root.sidebarOpen && root.isAuthenticated
        width: Math.min(300, root.width * 0.85)
        height: root.height
        z: 10
    }

    Rectangle {
        visible: root.isPhone && root.sidebarOpen && root.isAuthenticated
        x: sidebarOverlay.width
        y: 0
        width: root.width
        height: root.height
        color: "#80000000"
        z: 9
        MouseArea {
            anchors.fill: parent
            onClicked: root.sidebarOpen = false
        }
    }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "components"

Window {
    id: root
    visible: true
    width: 980; height: 720
    minimumWidth: 500; minimumHeight: 500
    title: "Hogrin Messenger"

    property bool   sidebarOpen:     false
    property bool   isPhone:         width < 600
    property bool   isTablet:        width >= 600 && width < 1024
    property bool   isAuthenticated: false
    property string loggedInUser:    ""
    property string activeChatUser:  ""
    property bool   bootstrapIsIPv4: false

    readonly property string currentUsername: loggedInUser

    Style { id: theme }
    ListModel { id: chatModel }

    // ── Load history ──────────────────────────────────────────────────────────
    onActiveChatUserChanged: {
        chatModel.clear()
        if (activeChatUser === "") return
        let history = backend.load_history(activeChatUser, 100)
        for (let i = 0; i < history.length; ++i) {
            let msg  = history[i]
            let date = new Date(msg.timestamp * 1000)
            chatModel.append({
                "msgId":            msg.id,
                "senderName":       msg.sender,
                "messageText":      msg.text,
                "isMe":             Boolean(msg.is_outgoing),
                "msgTime":          Qt.formatDateTime(date, "hh:mm"),
                "timestamp":        msg.timestamp,
                "msgStatus":        msg.status,
                "replyToId":        msg.reply_to_id,
                "replyToText":      msg.reply_to_text,
                "replyToSenderName": msg.reply_to_sender   // ← ключевое поле
            })
        }
    }

    function updateMessageStatus(msgId, status) {
        for (let i = chatModel.count - 1; i >= 0; --i) {
            if (chatModel.get(i).msgId === msgId) {
                chatModel.setProperty(i, "msgStatus", status)
                break
            }
        }
    }

    function markReadUpTo(peer, timestamp) {
        if (peer !== activeChatUser) return
        for (let i = 0; i < chatModel.count; ++i) {
            let m = chatModel.get(i)
            if (m.isMe && m.timestamp <= timestamp && m.msgStatus < 3)
                chatModel.setProperty(i, "msgStatus", 3)
        }
    }

    Component.onCompleted: {
        backend.check_saved_session()
        updateChecker.check_for_updates()
    }

    // ── Translations ──────────────────────────────────────────────────────────
    QtObject {
        id: tr
        readonly property string langCode: Qt.locale().name.substring(0, 2)
        readonly property var strings: ({
            "ru": { updateTitle:"Доступно обновление", updateQuestion:"Доступна версия %1.\nУстановить?", downloading:"Скачивание... %1%", downloadDone:"Готово к установке", installInfo:"Android попросит разрешение на установку из неизвестных источников — это нормально.", installBtn:"Установить", changelog:"Что нового:", error:"Ошибка загрузки" },
            "de": { updateTitle:"Update verfügbar", updateQuestion:"Version %1 ist verfügbar.\nJetzt installieren?", downloading:"Lade herunter... %1%", downloadDone:"Bereit zur Installation", installInfo:"Android fragt nach Erlaubnis — das ist normal.", installBtn:"Installieren", changelog:"Neuigkeiten:", error:"Fehler beim Herunterladen" },
            "en": { updateTitle:"Update available", updateQuestion:"Version %1 is available.\nInstall now?", downloading:"Downloading... %1%", downloadDone:"Ready to install", installInfo:"Android will ask for permission to install from unknown sources — this is expected.", installBtn:"Install", changelog:"What's new:", error:"Download failed" }
        })
        function t(key)       { let l = strings[langCode] ? langCode : "en"; return strings[l][key] || strings["en"][key] || key }
        function tf(key, arg) { return t(key).replace("%1", arg) }
    }

    // ── Update Dialog ─────────────────────────────────────────────────────────
    Dialog {
        id: updateDialog
        property string downloadUrl: ""; property string newVersion: ""; property string changelog: ""
        property int    downloadPercent: 0; property bool downloading: false; property bool readyToInstall: false

        anchors.centerIn: parent; title: tr.t("updateTitle"); modal: true
        closePolicy: (downloading || readyToInstall) ? Popup.NoAutoClose : Popup.CloseOnEscape
        standardButtons: (downloading || readyToInstall) ? Dialog.NoButton : (Dialog.Yes | Dialog.No)

        ColumnLayout {
            width: 300; spacing: 12
            Label { visible: !updateDialog.downloading && !updateDialog.readyToInstall; text: tr.tf("updateQuestion", updateDialog.newVersion); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { visible: !updateDialog.downloading && !updateDialog.readyToInstall && updateDialog.changelog !== ""; text: tr.t("changelog") + "\n" + updateDialog.changelog; wrapMode: Text.WordWrap; font.pixelSize: 12; opacity: 0.7; Layout.fillWidth: true }
            Label { visible: updateDialog.downloading; text: tr.tf("downloading", updateDialog.downloadPercent); Layout.fillWidth: true }
            ProgressBar { visible: updateDialog.downloading; Layout.fillWidth: true; from: 0; to: 100; value: updateDialog.downloadPercent }
            Rectangle {
                visible: updateDialog.readyToInstall; Layout.fillWidth: true; height: installCol.implicitHeight + 24; color: "#1a1a2e"; radius: 8
                ColumnLayout { id: installCol; anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 } spacing: 10
                    Label { text: "✓ " + tr.t("downloadDone"); font.bold: true; color: "#4caf50"; Layout.fillWidth: true }
                    Label { text: tr.t("installInfo"); wrapMode: Text.WordWrap; font.pixelSize: 12; opacity: 0.85; Layout.fillWidth: true }
                    Button { text: tr.t("installBtn"); Layout.fillWidth: true; onClicked: { updateChecker.trigger_install(); updateDialog.close() } }
                }
            }
        }
        onAccepted: { updateDialog.downloading = true; updateDialog.downloadPercent = 0; updateChecker.download_and_install(downloadUrl) }
        onRejected: { updateDialog.downloading = false; updateDialog.readyToInstall = false }
    }

    Connections {
        target: updateChecker
        function onUpdate_available(version, url, changelog) { updateDialog.newVersion = version; updateDialog.downloadUrl = url; updateDialog.changelog = changelog; updateDialog.downloading = false; updateDialog.readyToInstall = false; updateDialog.open() }
        function onDownload_progress(percent) { updateDialog.downloadPercent = percent }
        function onDownload_finished() { updateDialog.downloading = false; updateDialog.readyToInstall = true }
        function onDownload_failed(reason) { updateDialog.downloading = false; updateDialog.close() }
    }

    // ── Auth ──────────────────────────────────────────────────────────────────
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

        // 7 параметров: username, text, time, msg_id, reply_to_id, reply_to_text, reply_to_sender
        function onMessage_received(username, text, time, msg_id,
                                     reply_to_id, reply_to_text, reply_to_sender) {
            if (username === root.currentUsername) return
            if (username === root.activeChatUser) {
                chatModel.append({
                    "msgId":             msg_id,
                    "senderName":        username,
                    "messageText":       text,
                    "isMe":              false,
                    "msgTime":           Qt.formatDateTime(time, "hh:mm"),
                    "timestamp":         Math.floor(time.getTime() / 1000),
                    "msgStatus":         1,
                    "replyToId":         reply_to_id,
                    "replyToText":       reply_to_text,
                    "replyToSenderName": reply_to_sender    // ← имя отвечаемого
                })
            }
        }

        function onMessage_status_changed(msg_id, status) { updateMessageStatus(msg_id, status) }
        function onMessages_read_up_to(peer, timestamp)   { markReadUpTo(peer, timestamp) }
        function onBootstrap_transport_changed(is_ipv4)   { root.bootstrapIsIPv4 = is_ipv4 }
    }

    // ── Auth Screen ───────────────────────────────────────────────────────────
    AuthScreen {
        id: authScreen; anchors.fill: parent; visible: !root.isAuthenticated; z: 100
        onAuthSuccess: function(username) {
            root.loggedInUser = username; root.isAuthenticated = true
            backend.register_on_bootstrap(username)
            sidebar.loadSavedContacts(); sidebarOverlay.loadSavedContacts()
        }
    }

    // ── Main UI ───────────────────────────────────────────────────────────────
    SplitView {
        anchors.fill: parent; orientation: Qt.Horizontal; visible: root.isAuthenticated
        handle: Rectangle {
            implicitWidth: root.isPhone ? 0 : 4
            color: SplitHandle.hovered ? theme.accent : theme.border
            Behavior on color { ColorAnimation { duration: 150 } }
            HoverHandler { cursorShape: Qt.SplitHCursor }
            visible: !root.isPhone
        }
        ChatList { id: sidebar; SplitView.preferredWidth: isTablet ? 220 : 280; SplitView.minimumWidth: 180; SplitView.maximumWidth: 420; visible: !root.isPhone }
        ChatArea { SplitView.fillWidth: true; SplitView.minimumWidth: 280 }
    }

    ChatList { id: sidebarOverlay; visible: root.isPhone && root.sidebarOpen && root.isAuthenticated; width: Math.min(300, root.width * 0.85); height: root.height; z: 10 }
    Rectangle {
        visible: root.isPhone && root.sidebarOpen && root.isAuthenticated
        x: sidebarOverlay.width; y: 0; width: root.width; height: root.height; color: "#80000000"; z: 9
        MouseArea { anchors.fill: parent; onClicked: root.sidebarOpen = false }
    }
}
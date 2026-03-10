import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: authRoot
    color: theme.bgMain

    signal authSuccess(string username)

    property bool isLoginMode: true
    property string errorText: ""
    property bool isLoading: false

    // ── Асинхронні обробники авторизації ──
    Connections {
        target: backend

        function onLogin_result(success, error) {
            authRoot.isLoading = false
            if (success) {
                authRoot.errorText = ""
                backend.start_server(0)
                authRoot.authSuccess(nicknameInput.text.trim())
            } else {
                authRoot.errorText = error === "invalid"
                    ? "Invalid nickname or password" : error
            }
        }

        function onRegister_result(success, error) {
            if (success) {
                authRoot.errorText = ""
                backend.login_user(nicknameInput.text.trim(), passwordInput.text)
            } else {
                authRoot.isLoading = false
                authRoot.errorText = error === "already_exists"
                    ? "Nickname already taken" : error
            }
        }
    }

    // Background accent glow
    Rectangle {
        anchors.centerIn: parent
        width: 400; height: 400
        radius: 200
        color: theme.accentGlow
        opacity: 0.15
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(360, parent.width - 48)
        spacing: 0

        // ── Logo ──
        Column {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 60; height: 60
                radius: 16
                color: theme.bgActive

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#30FFFFFF" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "H"
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: theme.accent
                }
            }

            Item { height: 4 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Hogrin"
                font.pixelSize: 22
                font.weight: Font.Bold
                color: theme.text
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: isLoginMode ? "Welcome back" : "Create account"
                font.pixelSize: 13
                color: theme.textSecondary
            }
        }

        Item { height: 32 }

        // ── Form ──
        Rectangle {
            Layout.fillWidth: true
            height: 260
            radius: theme.radius
            color: theme.bgSide
            border.color: theme.border
            border.width: 1

            Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            ColumnLayout {
                id: formLayout
                anchors { fill: parent; margins: 20 }
                spacing: 12

                // Nickname field
                Column {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Nickname"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: theme.textSecondary
                    }

                    Rectangle {
                        width: parent.width
                        height: 42
                        radius: theme.radiusSmall
                        color: theme.bgInput
                        border.color: nicknameInput.activeFocus ? theme.accent : theme.border
                        border.width: nicknameInput.activeFocus ? 2 : 1
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        TextInput {
                            id: nicknameInput
                            anchors {
                                left: parent.left; right: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 14; rightMargin: 14
                            }
                            font.pixelSize: 14
                            color: theme.text
                            clip: true
                            enabled: !authRoot.isLoading

                            Text {
                                anchors.fill: parent
                                text: "Enter your nickname"
                                font: nicknameInput.font
                                color: theme.textSecondary
                                visible: nicknameInput.text.length === 0
                                verticalAlignment: Text.AlignVCenter
                            }

                            Keys.onReturnPressed: passwordInput.forceActiveFocus()
                        }
                    }
                }

                // Password field
                Column {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Password"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: theme.textSecondary
                    }

                    Rectangle {
                        width: parent.width
                        height: 42
                        radius: theme.radiusSmall
                        color: theme.bgInput
                        border.color: passwordInput.activeFocus ? theme.accent : theme.border
                        border.width: passwordInput.activeFocus ? 2 : 1
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        TextInput {
                            id: passwordInput
                            anchors {
                                left: parent.left; right: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 14; rightMargin: 14
                            }
                            font.pixelSize: 14
                            color: theme.text
                            clip: true
                            echoMode: TextInput.Password
                            enabled: !authRoot.isLoading

                            Text {
                                anchors.fill: parent
                                text: "Enter your password"
                                font: passwordInput.font
                                color: theme.textSecondary
                                visible: passwordInput.text.length === 0
                                verticalAlignment: Text.AlignVCenter
                            }

                            Keys.onReturnPressed: submitAuth()
                        }
                    }
                }

                // Error text
                Text {
                    visible: authRoot.errorText.length > 0
                    text: authRoot.errorText
                    font.pixelSize: 12
                    color: "#E05C5C"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }

                Item { height: 4 }

                // Submit button
                Rectangle {
                    Layout.fillWidth: true
                    height: 44
                    radius: theme.radiusSmall
                    color: submitArea.containsMouse ? theme.accentHover : theme.accent
                    Behavior on color { ColorAnimation { duration: 120 } }

                    opacity: authRoot.isLoading ? 0.5 : 1.0

                    scale: submitArea.containsMouse && !authRoot.isLoading ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#25FFFFFF" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: authRoot.isLoading ? "Please wait..." : (isLoginMode ? "Sign In" : "Create Account")
                        font.pixelSize: 14
                        font.weight: Font.SemiBold
                        color: "#FFFFFF"
                    }

                    MouseArea {
                        id: submitArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: !authRoot.isLoading
                        onClicked: submitAuth()
                    }
                }
            }
        }

        Item { height: 16 }

        // ── Switch mode ──
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6
            opacity: authRoot.isLoading ? 0.5 : 1.0

            Text {
                text: isLoginMode ? "Don't have an account?" : "Already have an account?"
                font.pixelSize: 13
                color: theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: isLoginMode ? "Sign up" : "Sign in"
                font.pixelSize: 13
                font.weight: Font.SemiBold
                color: theme.accent
                anchors.verticalCenter: parent.verticalCenter

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    enabled: !authRoot.isLoading
                    onClicked: {
                        isLoginMode = !isLoginMode
                        authRoot.errorText = ""
                        nicknameInput.text = ""
                        passwordInput.text = ""
                    }
                }
            }
        }
    }

    function submitAuth() {
        const nick = nicknameInput.text.trim()
        const pass = passwordInput.text

        if (nick.length === 0 || pass.length === 0) {
            authRoot.errorText = "Please fill in all fields"
            return
        }

        if (nick.length < 3) {
            authRoot.errorText = "Nickname must be at least 3 characters"
            return
        }

        if (pass.length < 6) {
            authRoot.errorText = "Password must be at least 6 characters"
            return
        }

        authRoot.isLoading = true
        authRoot.errorText = ""

        if (isLoginMode) {
            backend.login_user(nick, pass)
        } else {
            backend.register_user(nick, pass)
        }
    }
}

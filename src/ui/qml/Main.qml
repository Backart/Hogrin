import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    visible: true
    width: 400
    height: 600

    Connections{
        target: backend
        function onDisplay_message(text){
            chatModel.append({"modelData": text})
        }
    }

    ColumnLayout{
        anchors.fill: parent
        ListView{
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel{
                id: chatModel
            }
            delegate: Text {
                text: modelData
            }
        }
        RowLayout{
            TextField{
                id: messageInput
                Layout.fillWidth: true
            }
            Button{
                text: "->"
                onClicked: {
                    backend.send_message_from_ui(messageInput.text)
                    messageInput.text = ""
                }
            }
        }
    }
}

import QtQuick 2.0
import QtQuick.Controls 1.1
import QtQuick.Dialogs 1.2

Dialog {
    title: qsTr("Export Image")
    standardButtons: StandardButton.Ok | StandardButton.Cancel
    onAccepted: {
        exportImageWidth = parseInt(textField.text)
        imageSaveDialog.open()
    }
    onRejected: gameDisplay.forceActiveFocus() // QTBUG-48456

    Column {
        Label { text: qsTr("Image width:") }
        TextField {
            id: textField

            text: exportImageWidth
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator{ bottom: 0; top: 32767 }
        }
    }
}
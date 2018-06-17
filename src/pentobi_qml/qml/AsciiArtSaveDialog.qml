import QtQuick 2.0
import "Main.js" as Logic
import "." as Pentobi

Pentobi.FileDialog {
    title: qsTr("Export ASCII Art")
    selectExisting: false
    nameFilterText: qsTr("Text files (*.txt)")
    nameFilter: "*.png"
    folder: rootWindow.folder != "" ? rootWindow.folder : (isAndroid ? "file:///sdcard" : "")
    onAccepted: {
        rootWindow.folder = folder
        Logic.exportAsciiArt(fileUrl)
    }
}

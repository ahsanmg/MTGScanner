import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Pane {
    id: root

    property string channelName: "Channel-1"
    property bool channelRunning: false
    signal startChannel()
    signal stopChannel()

    RowLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.fillHeight: true

            text: root.channelName + " - Channel"
            font.pixelSize: 18
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        ToolButton {
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter

            icon.source: "qrc:/MTGScanner/icons/" + (root.channelRunning ? "pause.svg" : "play.svg")
            opacity: 0.8
            onClicked: root.channelRunning ? stopChannel() : startChannel()
        }
    }
}
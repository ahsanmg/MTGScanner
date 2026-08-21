import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

Pane {
    id: root

    Material.elevation: 2
    padding: 20

    property string description: "Description"
    property string cameraId: "camera_id"

    ColumnLayout {
        spacing: 12
        anchors.fill: parent

        // Header
        Label {
            text: "CAMERA SOURCE"
            font.weight: Font.DemiBold
            font.pixelSize: 12
            opacity: 0.6
        }

        // Description
        Label {
            text: root.description
            font.pixelSize: 20
            font.weight: Font.Bold
            elide: Label.ElideRight
            Layout.fillWidth: true
        }

        // ID
        Label {
            visible: root.cameraId !== ""
            text: root.cameraId
            font.pixelSize: 13
            opacity: 0.6
            elide: Label.ElideRight
            Layout.fillWidth: true
        }
    }
}
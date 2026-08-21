import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import MTGScanner

Pane {
    id: root

    Material.elevation: 2
    padding: 20

    property alias isWindowOpen: openCloseWindowButton.checked
    property string winName: "Window 1"
    property rect geometry
    property string screenName: "Primary 1"

    ColumnLayout {
        spacing: 12
        anchors.fill: parent

        RowLayout {
            Label {
                text: "OUTPUT WINDOW"
                font.weight: Font.DemiBold
                font.pixelSize: 12
                opacity: 0.6
            }

            Item { Layout.fillWidth: true; Layout.fillHeight: true }

            ToolButton {
                id: openCloseWindowButton
                checked: true
                checkable: true
                display: Button.IconOnly
                icon {
                    name: "window-new"
                    source: "qrc:/MTGScanner/icons/" + (checked ? "open-link-60.svg" : "open-link.svg")
                }
                opacity: 0.6
                Layout.preferredHeight: 32
                Layout.preferredWidth: 32
            }
        }

        Label {
            text: root.winName + " - " + root.screenName
            font.pixelSize: 14
            font.weight: Font.Medium
            color: Material.foreground
        }

        // Detail: Size & Position
        Label {
            text: root.geometry.width + "×" + root.geometry.height +
                    " · (" + root.geometry.x + ", " + root.geometry.y + ")"
            font.pixelSize: 14
            font.weight: Font.Medium
            color: Material.foreground
        }
    }
}
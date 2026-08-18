import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MTGScanner.Engine

Drawer {
    id: root

    property string activeChannelId: ""
    property alias channelModel: channelList.model
    signal addChannelClicked()
    signal deleteChannelClicked(id: string)

    Material.roundedScale: Material.SmallScale

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        // Header
        Label {
            Layout.fillWidth: true
            padding: 16

            text: "SCAN CHANNELS"
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        // Channels
        ListView {
            id: channelList
            
            Layout.fillWidth: true
            Layout.fillHeight: true

            highlightFollowsCurrentItem: true
            delegate: ItemDelegate {
                id: channelDelegate
                width: ListView.view.width
                height: 60
                horizontalPadding: 12

                contentItem: Item {
                    RowLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            Layout.alignment: Qt.AlignVCenter

                            text: (index + 1) + ". " + model.name
                            font: channelDelegate.font
                        }
                        Item { Layout.fillHeight: true; Layout.fillWidth: true } // spacer

                        ToolButton {
                            Layout.fillHeight: true
                            Layout.alignment: Qt.AlignVCenter

                            icon.source: "qrc:/MTGScanner/icons/trash-2.svg"
                            opacity: 0.6
                            visible: channelDelegate.hovered
                            onClicked: root.deleteChannelClicked(model.id)
                        }
                    }
                }

                onClicked: {
                    activeChannelId = model.id
                    channelList.currentIndex = index
                }
            }

            onCountChanged: {
                if (count === 0) {
                    activeChannelId = ""
                } else if (activeChannelId === "" || !Engine.channelExists(activeChannelId)) {
                    var firstChannel = Engine.channelAtIndex(0)
                    if (firstChannel)
                        activeChannelId = firstChannel.options.id
                }
            }
        }

        // Add channel button
        Button {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.preferredHeight: 60
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            Material.roundedScale: Material.SmallScale 

            flat: true
            highlighted: true
            text: "Add Channel"
            icon.source: "qrc:/MTGScanner/icons/plus.svg"
            onClicked: root.addChannelClicked()
        }
    }
}
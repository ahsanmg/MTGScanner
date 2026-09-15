import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import MTGScanner.Engine

Drawer {
    id: root

    property string activeChannelId: ""
    property alias channelModel: channelList.model

    signal addChannelClicked()
    signal deleteChannelClicked(string id)

    implicitWidth: 260
    Material.elevation: 4

    background: Rectangle {
        color: root.Material.backgroundColor
        border.color: root.Material.color(Material.Grey, Material.Shade800)
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Header Section
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 16
            spacing: 12

            RowLayout {
                spacing: 10

                Image {
                    width: 28; height: 28;
                    source: "qrc:/MTGScanner/icons/app.svg"
                    sourceSize.width: width
                    sourceSize.height: height
                }

                Label {
                    text: "MTGScanner"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: root.Material.foreground
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.Material.color(Material.Grey, Material.Shade800)
            }

            Label {
                text: "SCAN CHANNELS"
                font.pixelSize: 11
                font.weight: Font.Bold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
                color: root.Material.hintTextColor
            }
        }

        // Channels
        ListView {
            id: channelList

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 6
            clip: true

            delegate: ItemDelegate {
                id: channelDelegate

                required property int index
                required property var model
                readonly property bool isSelected: root.activeChannelId === model.id

                width: ListView.view.width
                height: 48
                horizontalPadding: 10
                highlighted: isSelected

                onClicked: {
                    root.activeChannelId = model.id
                    channelList.currentIndex = index
                }

                background: Rectangle {
                    radius: 8
                    color: channelDelegate.highlighted 
                        ? Qt.alpha(channelDelegate.Material.accent, 0.12)
                        : (channelDelegate.hovered ? channelDelegate.Material.rippleColor : "transparent")
                    border.color: channelDelegate.highlighted ? channelDelegate.Material.accent : "transparent"
                    border.width: 1
                }

                contentItem: RowLayout {
                    spacing: 10

                    Rectangle {
                        width: 24; height: 24; radius: 12
                        color: channelDelegate.highlighted 
                            ? channelDelegate.Material.accent 
                            : channelDelegate.Material.color(Material.Grey, Material.Shade800)

                        Label {
                            anchors.centerIn: parent
                            text: (channelDelegate.index + 1).toString()
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: channelDelegate.Material.foreground
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: model.name
                        font.pixelSize: 13
                        font.weight: channelDelegate.highlighted ? Font.Bold : Font.Normal
                        color: channelDelegate.Material.foreground
                        elide: Text.ElideRight
                    }

                    ToolButton {
                        id: deleteBtn
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 30; implicitHeight: 30
                        visible: channelDelegate.hovered

                        icon.source: "qrc:/MTGScanner/icons/trash-2.svg"
                        icon.color: deleteBtn.hovered 
                            ? deleteBtn.Material.color(Material.Red) 
                            : deleteBtn.Material.hintTextColor

                        onClicked: root.deleteChannelClicked(model.id)
                    }
                }
            }

            onCountChanged: {
                if (count === 0) {
                    root.activeChannelId = ""
                } else if (root.activeChannelId === "" || !Engine.channelExists(root.activeChannelId)) {
                    var firstChannel = Engine.channelAtIndex(0)
                    if (firstChannel)
                        root.activeChannelId = firstChannel.options.id
                }
            }
        }

        Button {
            id: addBtn

            Layout.fillWidth: true
            Layout.margins: 12
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignBottom

            text: "Add Channel"
            icon.source: "qrc:/MTGScanner/icons/plus.svg"

            font.pixelSize: 13
            font.weight: Font.Bold

            contentItem: RowLayout {
                Row {
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    spacing: 8

                    Image {
                        source: addBtn.icon.source
                        sourceSize.width: 18
                        sourceSize.height: 18
                        fillMode: Image.PreserveAspectFit
                        visible: source.toString() !== ""
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: addBtn.text
                        font: addBtn.font
                        color: "#FFFFFF"
                    }
                }
            }

            background: Rectangle {
                radius: 8
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: addBtn.hovered ? Qt.lighter("#EC4899", 1.1) : "#EC4899" }
                    GradientStop { position: 1.0; color: addBtn.hovered ? Qt.lighter("#8B5CF6", 1.1) : "#8B5CF6" }
                }
            }

            onClicked: root.addChannelClicked()
        }
    }
}
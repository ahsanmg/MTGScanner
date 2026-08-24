import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia
import MTGScanner
import MTGScanner.Engine
import MTGScanner.Components

Page {
    id: page
    title: "Configuration Page"

    property AbstractChannel channel: null
    property string prevChannelId

    onChannelChanged: {
        if (prevChannelId !== "") {
            Engine.unRegisterChannelOutSink(prevChannelId)
        }

        if (channel) {
            Engine.registerChannelOutSink(channel.options.id, videoCard.videoOutput.videoSink)
            prevChannelId = channel.options.id
        }
    }

    Component.onDestruction: {
        if (prevChannelId !== "") {
            Engine.unRegisterChannelOutSink(prevChannelId)
        }
    }

    Label {
        text: "No Channels yet. Please create one!"
        font.pixelSize: 16
        font.bold: true
        color: Material.hintTextColor
        visible: !channel

        anchors.centerIn: parent
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 16
        anchors.bottomMargin: 16
        spacing: 12
        visible: channel

        // Configuration Header
        HeaderSection {
            Layout.fillWidth: true
            Layout.preferredHeight: 50

            padding: 10
            leftPadding: 20
            rightPadding: 20
            Material.elevation: 2

            channelName: !channel ? "" : channel.options.name
            channelRunning: !channel ? false : channel.metrics.status === Engine.Running

            onStartChannel: {
                if (channel)
                    Engine.startChannel(channel.options.id)
            }
            onStopChannel: {
                if (channel)
                    Engine.stopChannel(channel.options.id)
            }
        }

        // Live Preview Section
        CameraPreviewCard {
            id: videoCard

            Layout.fillWidth: true
            Layout.fillHeight: true

            Component.onCompleted: {
                Engine.registerPredictionOverlay(overlay)
            }
        }

        // Configuration Section
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: 130
            spacing: 10

            CameraSourceCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                cameraId: channel ? channel.options.cameraDevice.id : "dummy_id"
                description: channel ? channel.options.cameraDevice.description : "Dummy Description"
            }

            OutputWindowCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                winName: channel ? channel.options.windowName : "Invalid Window Name"
                geometry: channel ? channel.options.windowGeometry : [-1, -1, -1, -1]
                screenName: channel ? channel.outputWindowScreen.name : "Invalid Screen"

                onIsWindowOpenChanged: isWindowOpen ? Engine.launchOutputWindow(channel.options.id)
                                        : Engine.closeOutputWindow(channel.options.id)
            }
        }

        // Status Section
        StatusSection {
            Layout.fillWidth: true
            Layout.preferredHeight: 50

            padding: 10
            leftPadding: 20
            rightPadding: 20
            Material.elevation: 2

            status: channel ? channel.metrics.status : 0
            statusColor: channel ? channel.metrics.statusColor : "red"
            fps: channel ? channel.metrics.fps : -1
            captureFps: channel ? channel.metrics.captureFps : -1
            skippedFps: channel ? channel.metrics.skippedFps : -1
            visibleCards: channel ? channel.metrics.visibleCards : -1
        }
    }
}

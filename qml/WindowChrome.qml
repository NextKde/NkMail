import QtQuick
import QtQuick.Window
import Kos.Ui

// Window-level interaction chrome. The shell owns its background planes;
// this component only supplies movement and native window controls.
Item {
    id: chrome
    required property var targetWindow
    objectName: "windowChrome"
    height: MailStyle.shellHeaderHeight
    readonly property real controlsReservedWidth: controls.width + 29
    readonly property real controlsCenterY: controls.y + controls.height / 2
    readonly property real controlsLeft: controls.x

    MouseArea {
        id: moveArea
        anchors.fill: parent
        anchors.rightMargin: controls.width + 22
        property point pressPosition
        property bool moveArmed: false

        onPressed: mouse => {
            pressPosition = Qt.point(mouse.x, mouse.y)
            moveArmed = true
        }
        onReleased: moveArmed = false
        onCanceled: moveArmed = false
        onPositionChanged: mouse => {
            if (pressed && moveArmed
                    && (Math.abs(mouse.x - pressPosition.x) > 4
                        || Math.abs(mouse.y - pressPosition.y) > 4)) {
                moveArmed = false
                chrome.targetWindow.startSystemMove()
            }
        }
        onDoubleClicked: {
            moveArmed = false
            chrome.targetWindow.toggleMaximized()
        }
    }

    KosWindowControls {
        id: controls
        objectName: "windowControls"
        targetWindow: chrome.targetWindow
        maximizeMode: true
        circleSize: 14
        buttonWidth: 14
        buttonHeight: 14
        spacing: 14
        anchors.right: parent.right
        anchors.rightMargin: 14.5
        anchors.top: parent.top
        anchors.topMargin: 10.5
    }
}

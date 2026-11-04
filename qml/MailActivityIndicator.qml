import QtQuick
import QtQuick.Window
import Kos.Ui

// Application activity feedback using ordinary scene items and GUI-thread
// NumberAnimation. It uses the same explicit animation on software and GPU
// renderers; no render-thread Animator or texture/shader is needed.
Item {
    id: root
    property bool running: true
    property color color: AppTheme.accent
    property bool reducedMotion: AppTheme.reduceMotion
    property string accessibleText: qsTr("Loading…")
    readonly property bool animating: running && visible && (!Window.window || Window.window.visible)
    property real phase: 0
    implicitWidth: 24
    implicitHeight: 24
    Accessible.role: Accessible.ProgressBar
    Accessible.name: accessibleText

    Item {
        id: wheel
        anchors.centerIn: parent
        width: Math.min(root.width, root.height)
        height: width
        rotation: root.reducedMotion ? 0 : root.phase * 360
        // Essential activity remains visible with reduced motion, using a slow
        // opacity pulse instead of continuous spatial movement.
        opacity: root.reducedMotion ? 0.55 + 0.45 * (1 - Math.cos(root.phase * Math.PI * 2)) / 2 : 1
        Repeater {
            model: 12
            Rectangle {
                id: spoke
                required property int index
                width: Math.max(2, wheel.width * 0.085)
                height: Math.max(3, wheel.height * 0.22)
                radius: width / 2
                x: (wheel.width - width) / 2
                y: wheel.height * 0.04
                color: root.color
                opacity: 0.18 + 0.82 * (index + 1) / 12
                transform: Rotation {
                    origin.x: spoke.width / 2
                    origin.y: wheel.height / 2 - wheel.height * 0.04
                    angle: spoke.index * 30
                }
            }
        }
    }
    NumberAnimation on phase {
        from: 0; to: 1
        duration: root.reducedMotion ? 1600 : 1000
        loops: Animation.Infinite
        running: root.animating
    }
}

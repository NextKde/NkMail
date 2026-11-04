import QtQuick
import Kos.Ui

WheelHandler {
    id: root
    required property var flickable
    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    property real destination: 0
    property NumberAnimation scrollAnimation: NumberAnimation {
        id: motion
        target: root.flickable
        property: "contentY"
        duration: 160
        easing.type: Easing.OutCubic
    }
    onWheel: function(event) {
        const view = flickable
        const delta = event.pixelDelta.y || event.angleDelta.y / 2
        if (!view || !delta) { event.accepted = false; return }
        const minimum = view.originY || 0
        const maximum = Math.max(minimum, minimum + view.contentHeight - view.height)
        const start = motion.running ? destination : view.contentY
        motion.stop()
        destination = Math.max(minimum, Math.min(maximum, start - delta))
        if (event.pixelDelta.y || AppTheme.reduceMotion) view.contentY = destination
        else { motion.from = view.contentY; motion.to = destination; motion.start() }
        event.accepted = true
    }
    property Connections dragConnection: Connections {
        target: root.flickable
        function onDraggingChanged() { if (root.flickable.dragging) motion.stop() }
    }
}

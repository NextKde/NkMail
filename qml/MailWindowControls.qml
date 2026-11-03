import QtQuick
import Kos.Ui

// Compatibility adapter for the pinned Kos.Ui release. Keep native protocol
// imports optional and keep the target opaque while only the fallback hides.
Item {
    id: root
    required property var targetWindow
    property alias maximizeMode: controls.maximizeMode
    property alias circleSize: controls.circleSize
    property alias buttonWidth: controls.buttonWidth
    property alias buttonHeight: controls.buttonHeight
    property alias spacing: controls.spacing
    property alias layoutDirection: controls.layoutDirection
    property Component platformComponent: Qt.createComponent("qrc:/nkmail/platform/NativeWindowControls.qml")
    property var platformSlot: null
    readonly property bool compositorActive: platformSlot !== null && platformSlot.active
    property int layoutRevision: 0
    readonly property var buttonLayout: {
        const revision = layoutRevision
        const result = []
        // Read the pinned component's actual delegates, not guessed offsets.
        const actions = {windowfullscreenButton: "maximize", windowminimizeButton: "minimize", windowcloseButton: "close"}
        for (const button of controls.children) {
            const action = actions[button.objectName]
            if (action && button.visible)
                result.push({action: action,
                    x: button.x + (button.width - circleSize) / 2,
                    y: button.y + (button.height - circleSize) / 2,
                    width: circleSize, height: circleSize})
        }
        return result
    }
    implicitWidth: controls.implicitWidth
    implicitHeight: controls.implicitHeight

    function connectPlatform() {
        if (platformSlot) {
            platformSlot.destroy()
            platformSlot = null
        }
        if (platformComponent && platformComponent.status === Component.Ready)
            platformSlot = platformComponent.createObject(root, {
                target: root,
                buttons: Qt.binding(() => root.buttonLayout),
                enabled: Qt.binding(() => root.maximizeMode)
            })
    }
    onPlatformComponentChanged: connectPlatform()
    Component.onCompleted: connectPlatform()
    Connections {
        target: root.platformComponent
        function onStatusChanged() { root.connectPlatform() }
    }
    KosWindowControls {
        id: controls
        objectName: "clientWindowControls"
        targetWindow: root.targetWindow
        opacity: root.compositorActive ? 0 : 1
        enabled: !root.compositorActive
        Accessible.ignored: root.compositorActive
        onPositioningComplete: root.layoutRevision++
    }
}

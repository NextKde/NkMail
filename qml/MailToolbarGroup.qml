import QtQuick
import QtQuick.Controls
import Kos.Ui

// Mail-specific horizontal layout; Kos.Ui owns interaction, focus and theming.
Item {
    id: root
    property var icons: []
    property var labels: []
    property var objectNames: []
    property var enabledActions: []
    property bool showLabels: true
    property bool materialForm: false
    property LiquidBackdrop backdrop: null
    signal triggered(int index)
    implicitWidth: actions.implicitWidth
    implicitHeight: 28

    // One canonical glass surface for the entire action group.
    LiquidActionButton {
        id: groupSurface
        anchors.fill: parent
        enabled: false
        Accessible.ignored: true
        materialForm: root.materialForm
        backdrop: root.backdrop
    }

    Row {
        id: actions
        height: root.height
        spacing: 0

        Repeater {
            model: root.icons.length
            LiquidActionButton {
                id: action
                required property int index
                objectName: root.objectNames[index] || ""
                enabled: root.enabledActions[index] !== false
                height: root.height
                width: root.showLabels ? Math.ceil(inlineContent.implicitWidth + 20) : 28
                materialForm: true
                flat: true
                surfaceColor: "transparent"
                // Transparent action cells sit on the group's shared glass.
                textColor: !enabled ? AppTheme.disabledText : root.materialForm
                    ? AppTheme.text : (AppTheme.dark ? "#D6D6DA" : "#55555A")
                // A segment is not a separate capsule: match the shared surface
                // only at the group's outside ends. Keep glyphs unscaled.
                scale: 1
                background: Item {
                    clip: true
                    Rectangle {
                        readonly property bool outerEnd: action.index === 0 || action.index === root.icons.length - 1
                        radius: outerEnd ? height / 2 : 0
                        x: action.index > 0 && action.index === root.icons.length - 1 ? -radius : 0
                        width: parent.width + (outerEnd && root.icons.length > 1 ? radius : 0)
                        height: parent.height
                        color: AppTheme.text
                        opacity: !action.enabled ? 0 : action.down ? 0.13 : action.hovered ? 0.07 : 0
                        antialiasing: true
                    }
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 2
                        radius: 4
                        color: "transparent"
                        border.width: 2
                        border.color: AppTheme.focusRing
                        visible: action.visualFocus
                        antialiasing: true
                    }
                }
                Accessible.name: root.labels[index] || root.icons[index]
                contentItem: Row {
                    id: inlineContent
                    spacing: 5
                    VectorIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: root.icons[action.index]
                        size: 14
                        color: action.textColor
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.showLabels
                        text: root.labels[action.index] || ""
                        color: action.textColor
                        font.pixelSize: 11
                    }
                }
                Rectangle {
                    visible: action.index > 0
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 12
                    color: AppTheme.withAlpha(AppTheme.text, 0.12)
                }
                KosToolTip { target: action; text: action.Accessible.name }
                onClicked: root.triggered(index)
            }
        }
    }
}

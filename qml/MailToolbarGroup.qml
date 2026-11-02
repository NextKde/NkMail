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
                width: root.showLabels ? inlineContent.implicitWidth + 20 : 28
                materialForm: true
                flat: true
                surfaceColor: "transparent"
                // Transparent action cells sit on the group's shared glass.
                textColor: !enabled ? AppTheme.disabledText : root.materialForm
                    ? AppTheme.text : (AppTheme.dark ? "#D6D6DA" : "#55555A")
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

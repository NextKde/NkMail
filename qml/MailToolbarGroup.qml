import QtQuick
import QtQuick.Controls
import Kos.Ui

// Mail-specific horizontal layout; Kos.Ui owns interaction, focus and theming.
Row {
    id: root
    property var icons: []
    property var labels: []
    property var objectNames: []
    property var enabledActions: []
    property bool showLabels: true
    property bool materialForm: false
    property LiquidBackdrop backdrop: null
    signal triggered(int index)
    spacing: 2
    height: 30

    Repeater {
        model: root.icons.length
        LiquidActionButton {
            id: action
            required property int index
            objectName: root.objectNames[index] || ""
            enabled: root.enabledActions[index] !== false
            height: 30
            width: root.showLabels ? inlineContent.implicitWidth + 16 : 30
            materialForm: root.materialForm
            backdrop: root.backdrop
            flat: false
            Accessible.name: root.labels[index] || root.icons[index]
            contentItem: Row {
                id: inlineContent
                spacing: 6
                VectorIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: root.icons[action.index]
                    size: 15
                    color: action.textColor
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.showLabels
                    text: root.labels[action.index] || ""
                    color: action.textColor
                    font.pixelSize: 12
                }
            }
            KosToolTip { target: action; text: action.Accessible.name }
            onClicked: root.triggered(index)
        }
    }
}

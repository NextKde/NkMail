import QtQuick
import QtQuick.Controls
import Kos.Ui

// Compact, solid toolbar controls; material rules remain owned by Kos.Ui.
LiquidActionButton {
    id: root
    property string accessibleText: text
    materialForm: true
    implicitHeight: 26
    cornerRadius: 6
    flat: true
    font.pixelSize: 12
    Accessible.name: accessibleText
    ToolTip.visible: hovered && accessibleText.length > 0
    ToolTip.text: accessibleText
    ToolTip.delay: 600
}

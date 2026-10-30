import QtQuick
import QtQuick.Controls
import Kos.Ui

// One compact semantic action adapter. Placement decides only the size;
// material, focus and tooltip behavior stay with Kos.Ui.
LiquidActionButton {
    id: root
    property string accessibleText: text
    property bool toolbarAction: true
    property bool primaryAction: false
    // Floating shell actions opt into the shared host-owned scene capture.
    // Form and dialog actions remain solid unless explicitly promoted.
    property bool floatingAction: false
    property LiquidBackdrop shellBackdrop: null

    materialForm: !root.floatingAction || AppTheme.materialMode === "solid"
        || AppTheme.reduceTransparency || GraphicsInfo.api === GraphicsInfo.Software
    backdrop: root.shellBackdrop
    highlighted: primaryAction
    implicitHeight: toolbarAction ? MailStyle.toolbarButtonHeight : 28
    cornerRadius: materialForm && height < 28 ? 6 : height / 2
    font.pixelSize: AppTheme.compactControlFontSize
    iconSize: 16
    Accessible.name: accessibleText

    KosToolTip {
        target: root
        text: root.accessibleText
    }
}

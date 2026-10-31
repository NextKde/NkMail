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

    // NKMAIL_FORCE_GLASS=1 仅用于预览：软件渲染下也保持液态形态（shader 仍不可用）
    materialForm: (!root.floatingAction || AppTheme.materialMode === "solid"
        || AppTheme.reduceTransparency
        || (GraphicsInfo.api === GraphicsInfo.Software
            && !(typeof forceGlassPreview !== "undefined" && forceGlassPreview)))
    backdrop: root.shellBackdrop
    highlighted: primaryAction
    TextMetrics {
        id: stackedTextMetrics
        text: root.stackedText
        font.pixelSize: root.stackedTextSize
    }
    implicitWidth: root.stacked ? Math.max(56, stackedTextMetrics.width + 22)
        : iconOnly ? implicitHeight : Math.max(76, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: root.stacked ? 36 : (toolbarAction ? MailStyle.toolbarButtonHeight : 28)
    // Respect the existing header without changing its height or the call site.
    Binding {
        target: root; property: "implicitHeight"
        value: 36; when: root.stacked
        restoreMode: Binding.RestoreBindingOrValue
    }
    Binding {
        target: root; property: "y"
        value: root.parent ? (root.parent.height - root.height) / 2 : 0
        when: root.stacked
        restoreMode: Binding.RestoreBindingOrValue
    }
    cornerRadius: root.stacked ? height / 2 : (materialForm && height < 28 ? 6 : height / 2)
    font.pixelSize: AppTheme.compactControlFontSize
    iconSize: 16
    Accessible.name: accessibleText

    KosToolTip {
        target: root
        text: root.accessibleText
    }
}

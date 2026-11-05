import QtQuick
import Kos.Ui

// Compact trailing editor for a SettingsItem; standalone block inputs stay plain.
SettingsTextField {
    id: editor
    implicitHeight: 20
    topPadding: 1
    bottomPadding: 1
    filled: true
    fillColor: "#d1d1d6"
    color: AppTheme.blackSeed
    placeholderTextColor: AppTheme.mix(color, fillColor, 0.25)
    selectionColor: AppTheme.defaultAccentSeed
    selectedTextColor: AppTheme.whiteSeed
    background: Rectangle {
        radius: 6
        color: editor.fillColor
        border.width: editor.activeFocus ? 1 : 0
        border.color: AppTheme.defaultAccentSeed
    }
}

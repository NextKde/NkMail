import QtQuick
import Kos.Ui

// Compact trailing editor for a SettingsItem; standalone block inputs stay plain.
SettingsTextField {
    implicitHeight: 20
    topPadding: 1
    bottomPadding: 1
    filled: true
    fillColor: activeFocus ? AppTheme.defaultAccentSeed : "#d1d1d6"
    color: activeFocus ? AppTheme.whiteSeed : AppTheme.blackSeed
    placeholderTextColor: AppTheme.mix(color, fillColor, 0.25)
    selectionColor: activeFocus ? AppTheme.whiteSeed : AppTheme.defaultAccentSeed
    selectedTextColor: activeFocus ? AppTheme.defaultAccentSeed : AppTheme.whiteSeed
}

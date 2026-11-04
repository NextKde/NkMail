import QtQuick
import Kos.Ui

// Compact trailing editor for a SettingsItem; standalone block inputs stay plain.
SettingsTextField {
    implicitHeight: 20
    topPadding: 1
    bottomPadding: 1
    filled: true
    fillColor: AppTheme.dark ? "#2C2C2E" : "#FFFFFF"
}

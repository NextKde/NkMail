pragma Singleton

import QtQuick
import Kos.Ui

// NkMail layout tokens. Semantic colours remain owned by AppTheme.
QtObject {
    readonly property int chromeHeight: 30
    readonly property int shellHeaderHeight: 44
    readonly property int toolbarHeight: 44
    readonly property int toolbarButtonHeight: 26
    readonly property int fieldHeight: 28
    readonly property int messageRowHeight: 84
    readonly property int draftRowHeight: 72
    readonly property int contentInset: 18
    readonly property int formSpacing: 8
    readonly property int footerHeight: 62
    readonly property real readerMaximumWidth: 720

    readonly property color window: AppTheme.contentBackground
    readonly property color sidebar: AppTheme.sidebar
    readonly property color surface: AppTheme.contentBackground
    readonly property color secondarySurface: AppTheme.contentSecondaryBackground
    readonly property color text: AppTheme.text
    readonly property color mutedText: AppTheme.mutedText
    readonly property color border: AppTheme.border
    readonly property color hover: AppTheme.cardHover
    readonly property color selected: AppTheme.withAlpha(AppTheme.accent, AppTheme.dark ? 0.18 : 0.10)
    readonly property color selectedBorder: AppTheme.withAlpha(AppTheme.accent, AppTheme.dark ? 0.42 : 0.28)
    readonly property color avatar: AppTheme.cardSurface
    readonly property color unread: AppTheme.accent
    readonly property color attachment: AppTheme.mutedText
}

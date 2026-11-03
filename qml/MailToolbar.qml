import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

Item {
    id: readerHeader
    required property var mail
    required property QtObject selection
    required property Item backdrop
    required property Item surface
    property real windowWidth: 1280
    property real controlsLeft: width
    property real controlsCenterY: height / 2
    property bool narrow: false
    property bool sidebarVisible: true
    property bool readerVisible: true
    property bool draftsVisible: false
    property bool liquidControls: true
    property bool hasMessage: false
    readonly property color muted: AppTheme.mutedText
    readonly property bool searchActive: toolbarSearch.active
    property alias optionsAnchor: messageOptionsGroup
    signal messageCommandRequested(int command)
    signal composeRequested(string mode)
    signal optionsRequested()
    signal moreRequested()
    signal detailsRequested()
    signal searchRequested()
    signal backRequested()
    signal layoutChanged()
    function focusWithin(item) { return item.activeFocus }

    objectName: "readerHeader"
    readonly property real leadingInset: readerHeader.narrow && !readerHeader.sidebarVisible ? 48 : 14
    readonly property real trailingEdge: Math.min(width, readerHeader.controlsLeft) - 10
    readonly property real rowWidth: Math.max(0, trailingEdge - leadingInset)
    readonly property real titleWidth: 0
    readonly property real actionLeft: leadingInset + titleWidth + (titleWidth > 0 ? 8 : 0)
    readonly property real actionWidth: Math.max(0, trailingEdge - actionLeft)
    readonly property bool messageActions: (!readerHeader.narrow || readerHeader.readerVisible) && !readerHeader.draftsVisible
    readonly property bool backVisible: false
    readonly property real baselineWidth: readerHeader.draftsVisible ? 52 : 60 + (messageActions ? 34 : 0) + (backVisible ? 34 : 0)
    readonly property bool compactActions: !readerHeader.draftsVisible && actionWidth < baselineWidth + 60
    readonly property bool showReplyGroup: messageActions && actionWidth >= baselineWidth + 120
    readonly property bool fullReplyGroup: messageActions && actionWidth >= baselineWidth + 146
    readonly property bool directDetails: messageActions && actionWidth >= baselineWidth + 180
    readonly property bool directFilter: !readerHeader.draftsVisible && actionWidth >= baselineWidth + 60 + (messageActions ? 120 : 0) + 34
    readonly property bool searching: searchOpen && !wideSearch
    readonly property bool compactOptions: actionWidth < 240
    readonly property bool extendedOptions: windowWidth >= 1180
    readonly property bool messageOpsVisible: messageOpsGroup.visible
    readonly property real searchReservation: (wideSearch ? 160 : MailStyle.toolbarButtonHeight) + 8
    readonly property real baseActionsWidth: (mailboxActions.visible ? mailboxActions.implicitWidth + 14 : 0)
        + messageOptionsGroup.implicitWidth + 14 + searchReservation
    // 宽度够就把搜索做成常显输入框，不够就退化成搜索按钮
    readonly property bool wideSearch: readerHeader.windowWidth >= 1320
    property bool searchOpen: false
    onCompactActionsChanged: {
        if (readerHeader.focusWithin(mailboxActions)) readerHeader.forceActiveFocus()
        readerHeader.layoutChanged()
    }
    onFullReplyGroupChanged: {
        if (readerHeader.focusWithin(replyActions)) readerHeader.forceActiveFocus()
    }
    function openSearch() {
        readerHeader.searchRequested()
        readerHeader.searchOpen = true
        searchFocus.restart()
    }
    function closeSearch() {
        searchFocus.stop()
        searchDelay.stop()
        readerHeader.searchOpen = false
        searchField.text = ""
        mail.search = ""
        searchField.focus = false
    }
    RowLayout {
        x: readerHeader.actionLeft
        y: readerHeader.controlsCenterY - height / 2
        width: readerHeader.actionWidth
        height: MailStyle.toolbarButtonHeight; spacing: 0
        MailAction {
            Layout.rightMargin: 8
            objectName: "readerBackButton"
            visible: readerHeader.backVisible && !readerHeader.searching
            iconOnly: true; iconName: "back"; accessibleText: qsTr("Back to messages")
            floatingAction: true; shellBackdrop: readerHeader.backdrop
            onClicked: readerHeader.backRequested()
        }
        MailToolbarGroup {
            backdrop: readerHeader.backdrop
            materialForm: !readerHeader.liquidControls
            id: mailboxActions
            Layout.rightMargin: 14
            showLabels: true
            objectName: "mailboxActionGroup"
            visible: !readerHeader.compactActions && !readerHeader.searching && readerHeader.windowWidth >= 1180
            icons: ["refreshRounded", "editRounded"]
            labels: [qsTr("收取"), qsTr("撰写")]
            objectNames: ["refreshButton", "headerComposeButton"]
            enabledActions: [!mail.busy && !readerHeader.draftsVisible && (mail.configured || mail.demo), mail.configured || mail.demo]
            onTriggered: index => {
                if (index === 0) mail.refresh()
                else readerHeader.composeRequested("new")
            }
        }
        MailToolbarGroup {
            backdrop: readerHeader.backdrop
            materialForm: !readerHeader.liquidControls
            id: messageOpsGroup
            showLabels: true
            objectName: "messageOpsGroup"
            Layout.rightMargin: 14
            visible: !readerHeader.draftsVisible && !readerHeader.searching && readerHeader.windowWidth >= 1100 && readerHeader.selection.selectionSize === 0
                && readerHeader.actionWidth >= readerHeader.baseActionsWidth + implicitWidth + 14
            icons: ["trash", "archive", "flag"]
            labels: [qsTr("删除"), qsTr("归档"), qsTr("旗标")]
            enabledActions: [readerHeader.hasMessage, readerHeader.hasMessage, readerHeader.hasMessage]
            onTriggered: index => {
                readerHeader.messageCommandRequested(index)
            }
        }
        Text {
            objectName: "selectionCountLabel"
            visible: readerHeader.selection.selectionSize > 0 && !readerHeader.searching
            text: qsTr("%1 selected").arg(readerHeader.selection.selectionSize)
            color: readerHeader.muted; font.pixelSize: 11
            Layout.alignment: Qt.AlignVCenter
        }
        MailToolbarGroup {
            backdrop: readerHeader.backdrop
            materialForm: !readerHeader.liquidControls
            id: selectionOpsGroup
            showLabels: true
            objectName: "selectionOpsGroup"
            visible: !readerHeader.draftsVisible && !readerHeader.searching && readerHeader.selection.selectionSize > 0
            icons: ["envelope", "flag", "archive", "trash", "folder"]
            labels: [qsTr("已读"), qsTr("旗标"), qsTr("归档"), qsTr("删除"), qsTr("移动")]
            onTriggered: index => {
                if (index === 0) mail.setMessagesRead(readerHeader.selection.selectedIndexList(), true)
                else if (index === 1) mail.setMessagesFlagged(readerHeader.selection.selectedIndexList(), true)
                else if (index === 2) {
                    mail.archiveMessages(readerHeader.selection.selectedIndexList())
                    readerHeader.selection.clearSelection()
                } else if (index === 3) {
                    mail.deleteMessages(readerHeader.selection.selectedIndexList())
                    readerHeader.selection.clearSelection()
                } else if (index === 4) selectionMoveMenu.openAtAnchor()
            }
        }
        LiquidPopover {
            id: selectionMoveMenu
            objectName: "selectionMoveMenu"
            sourceItem: readerHeader.surface
            anchorItem: selectionOpsGroup
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
            Repeater {
                model: mail.folders.filter(f => f.id !== mail.currentFolder)
                LiquidPopoverItem {
                    text: modelData.title || modelData.id
                    iconName: "folder"
                    onTriggered: {
                        selectionMoveMenu.close()
                        mail.moveMessages(readerHeader.selection.selectedIndexList(), modelData.id)
                        readerHeader.selection.clearSelection()
                    }
                }
            }
        }
        MailToolbarGroup {
            backdrop: readerHeader.backdrop
            materialForm: !readerHeader.liquidControls
            id: replyActions
            Layout.rightMargin: 14
            showLabels: true
            objectName: "replyActionGroup"
            visible: readerHeader.showReplyGroup && !readerHeader.searching && readerHeader.windowWidth >= 1480
                && readerHeader.actionWidth >= readerHeader.baseActionsWidth
                    + (messageOpsGroup.visible ? messageOpsGroup.implicitWidth + 14 : 0) + implicitWidth + 14
            icons: readerHeader.fullReplyGroup ? ["undo", "arrows", "forward"] : ["undo", "forward"]
            labels: readerHeader.fullReplyGroup ? [qsTr("回复"), qsTr("全部回复"), qsTr("转发")] : [qsTr("回复"), qsTr("转发")]
            objectNames: readerHeader.fullReplyGroup ? ["replyButton", "replyAllButton", "forwardButton"] : ["replyButton", "forwardButton"]
            enabledActions: [readerHeader.hasMessage && !mail.sending, readerHeader.hasMessage && !mail.sending, readerHeader.hasMessage && !mail.sending]
            onTriggered: index => readerHeader.composeRequested(readerHeader.fullReplyGroup ? ["reply", "replyAll", "forward"][index] : ["reply", "forward"][index])
            onVisibleChanged: { if (!visible && readerHeader.focusWithin(replyActions)) readerHeader.forceActiveFocus() }
        }
        MailToolbarGroup {
            backdrop: readerHeader.backdrop
            materialForm: !readerHeader.liquidControls
            id: messageOptionsGroup
            showLabels: true
            objectName: "messageOptionsGroup"
            Layout.rightMargin: 14
            visible: !readerHeader.draftsVisible && !readerHeader.searching
            icons: readerHeader.compactOptions ? ["more"] : ["sliders", "more"]
            labels: readerHeader.compactOptions ? [qsTr("更多")] : [qsTr("显示"), qsTr("更多")]
            objectNames: readerHeader.compactOptions ? ["moreButton"] : ["listOptionsButton", "moreButton"]
            enabledActions: readerHeader.compactOptions ? [true] : [true, true]
            onTriggered: index => {
                if (readerHeader.compactOptions) readerHeader.moreRequested()
                else if (index === 0) readerHeader.optionsRequested()
                else readerHeader.moreRequested()
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: true
        }
        Item {
            id: toolbarSearch
            objectName: "toolbarSearch"
            readonly property bool active: readerHeader.searchOpen || searchField.activeFocus
            readonly property real wanted: 160
            readonly property bool shown: !readerHeader.draftsVisible
                && (readerHeader.wideSearch || readerHeader.searchOpen)

            Layout.preferredWidth: shown ? wanted : 0
            Layout.fillWidth: readerHeader.searching
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            Layout.rightMargin: 0
            visible: shown
                clip: true
                LiquidTextField {
                    id: searchField
                    objectName: "searchField"
                    anchors.fill: parent
                    implicitHeight: MailStyle.toolbarButtonHeight
                    enabled: !readerHeader.draftsVisible
                    materialForm: !readerHeader.liquidControls
                    liquidFinish: true
                    liquidIntensity: "light"
                    backdrop: readerHeader.backdrop
                    leftPadding: 30
                    rightPadding: readerHeader.searching ? 36 : 10
                    placeholderText: qsTr("Search cached mail")
                    Accessible.name: placeholderText
                    text: mail.search
                    onTextEdited: searchDelay.restart()
                    VectorIcon {
                        x: 8; anchors.verticalCenter: parent.verticalCenter
                        name: "search"; size: 14; color: searchField.textColor
                        opacity: mail.busy ? 0 : 1
                    }
                    BusyIndicator {
                        x: 5; anchors.verticalCenter: parent.verticalCenter
                        width: 20; height: 20
                        visible: mail.busy
                        running: visible
                    }
                    Timer { id: searchDelay; interval: 200; onTriggered: mail.search = searchField.text }
                    onActiveFocusChanged: {
                        if (!activeFocus && !readerHeader.wideSearch && text.length === 0)
                            readerHeader.searchOpen = false
                    }
                }
                MailAction {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    objectName: "closeSearchButton"
                    visible: readerHeader.searching
                    iconOnly: true; iconName: "close"; iconSize: 14
                    accessibleText: qsTr("Close search")
                    floatingAction: true; shellBackdrop: readerHeader.backdrop
                    onClicked: readerHeader.closeSearch()
                }
            }
        MailAction {
            objectName: "searchToggleButton"
            Layout.rightMargin: 0
            visible: !readerHeader.wideSearch && !readerHeader.searchOpen && !readerHeader.draftsVisible
            iconOnly: true
            iconName: "search"
            iconSize: 14
            accessibleText: qsTr("Search cached mail")
            floatingAction: true; shellBackdrop: readerHeader.backdrop
            onClicked: readerHeader.openSearch()
        }

    }
    Timer {
        id: searchFocus
        interval: AppTheme.reduceMotion ? 0 : AppTheme.motionFast + 50
        onTriggered: { if (toolbarSearch.active && readerHeader.visible) searchField.forceActiveFocus() }
    }
}

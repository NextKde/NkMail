import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import Kos.Ui

ApplicationWindow {
    id: window
    required property var mail
    objectName: "mailWindow"
    width: 1280
    height: 820
    minimumWidth: 900
    minimumHeight: 520
    visible: true
    title: qsTr("NkMail")
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    background: Item {}
    font.family: "sans-serif"
    font.pixelSize: AppTheme.compactControlFontSize

    readonly property bool narrow: false // Narrow layout removed; minimum width is 900.
    readonly property bool liquidControls: AppTheme.materialMode !== "solid"
        && !AppTheme.reduceTransparency
        && GraphicsInfo.api !== GraphicsInfo.Software
    property bool sidebarExpanded: false
    property bool sidebarCollapsed: false
    property bool readerVisible: false
    property bool draftsVisible: false
    property bool showSummary: true
    property bool groupThreads: true
    property bool showRichText: true
    property var selectionIndices: ({})      // 行号 -> true
    property int selectionAnchor: -1
    readonly property int selectionSize: Object.keys(selectionIndices).length
    function isRowSelected(row) { return selectionIndices[row] === true }
    function clearSelection() { selectionIndices = ({}); selectionAnchor = -1 }
    function toggleRow(row) {
        const next = Object.assign({}, selectionIndices)
        if (next[row]) delete next[row]; else next[row] = true
        selectionIndices = next
        selectionAnchor = row
    }
    function selectRangeTo(row) {
        if (selectionAnchor < 0 || selectionAnchor === row) { toggleRow(row); return }
        const from = Math.min(selectionAnchor, row), to = Math.max(selectionAnchor, row)
        const next = Object.assign({}, selectionIndices)
        for (let i = from; i <= to; ++i) next[i] = true
        selectionIndices = next
    }
    function selectedIndexList() {
        const rows = Object.keys(selectionIndices).map(Number).sort((a, b) => a - b)
        return rows
    }
    property var collapsedThreads: ({})
    function toggleThread(key) {
        const next = Object.assign({}, collapsedThreads)
        if (next[key]) delete next[key]
        else next[key] = true
        collapsedThreads = next
    }
    property var collapsedAccounts: ({})     // 账号 id -> true
    function toggleAccountCollapsed(id) {
        const next = Object.assign({}, collapsedAccounts)
        if (next[id]) delete next[id]
        else next[id] = true
        collapsedAccounts = next
    }
    // The demo/preview account is not persisted in the accounts table, so fall
    // back to the active account while the cache has no account rows yet.
    readonly property var sidebarAccounts: {
        const list = (window.mail.accounts || [])
        if (list.length > 0) return list
        const current = window.mail.account
        return (current && String(current.address || "").length > 0) ? [current] : []
    }
    // Cache counters are read on demand; this bump makes the sidebar bindings
    // re-query them whenever the controller reports a change.
    property int unreadRevision: 0
    property bool detailsVisible: false
    Connections {
        target: window.mail
        function onFolderUnreadChanged() { window.unreadRevision++ }
    }
    onDraftsVisibleChanged: {
        listOptions.close()
        if (draftsVisible) readerHeader.closeSearch()
    }
    onReaderVisibleChanged: { listOptions.close(); moreMenu.close(); window.detailsVisible = false }
    function focusWithin(item) {
        let focused = activeFocusItem
        while (focused) {
            if (focused === item) return true
            focused = focused.parent
        }
        return false
    }
    // Preferred widths survive temporary narrow layouts; actual widths always
    // leave space for the reader and never squeeze the list below its minimum.
    property int sidebarPreferredWidth: 236
    property int listPreferredWidth: 320
    readonly property int sidebarWidth: Math.max(190, Math.min(340, sidebarPreferredWidth,
        width - (narrow ? 260 : 560)))
    readonly property int listWidth: Math.max(260, Math.min(520, listPreferredWidth,
        width - (sidebarVisible ? sidebarWidth : 0) - 300))
    readonly property bool sidebarVisible: width >= 900 ? !sidebarCollapsed : sidebarExpanded
    readonly property var message: mail.selectedMessage || ({})
    readonly property color selectedSurface: AppTheme.withAlpha(AppTheme.accent, AppTheme.dark ? 0.18 : 0.10)
    readonly property color hoverSurface: AppTheme.cardHover
    readonly property bool hasMessage: String(mail.selectedUid || "").length > 0
    readonly property color ink: AppTheme.text
    readonly property color muted: AppTheme.mutedText
    readonly property color rule: AppTheme.border
    readonly property color sidebarColor: AppTheme.sidebar

    function toggleSidebar() {
        if (width >= 1080) sidebarCollapsed = !sidebarCollapsed
        else sidebarExpanded = !sidebarExpanded
    }
    function value(map, key) { return map && map[key] !== undefined ? String(map[key]) : "" }
    function selectedIndex() {
        if (!hasMessage) return -1
        return mail.messages.indexOf(mail.currentFolder, window.value(message, "uidValidity"), mail.selectedUid)
    }
    function mailboxTitle(folder) {
        if (String(folder.id).toUpperCase() === "INBOX") return qsTr("Inbox")
        const title = String(folder.title || folder.id || "")
        if (title === "Sent") return qsTr("Sent")
        if (title === "Archive") return qsTr("Archive")
        if (title === "Junk") return qsTr("Junk")
        if (title === "Trash") return qsTr("Trash")
        return title
    }
    function folderIcon(id) {
        const name = String(id || "").split(/[\/\\]/).pop().toLowerCase()
        const starts = (prefix) => name.indexOf(prefix) === 0
        if (starts("inbox") || name.indexOf("收件") >= 0) return "envelope"
        if (starts("sent") || name.indexOf("已发送") >= 0 || name.indexOf("发送") >= 0) return "forward"
        if (starts("draft") || name.indexOf("草稿") >= 0) return "editRounded"
        if (starts("archive") || name.indexOf("归档") >= 0) return "archive"
        if (starts("trash") || name.indexOf("删除") >= 0) return "trash"
        if (starts("junk") || name.indexOf("垃圾") >= 0) return "flag"
        return "folder"
    }
    function draftStateLabel(state) {
        switch (state) {
        case "draft": return qsTr("Draft")
        case "sending": return qsTr("Sending")
        case "accepted": return qsTr("Accepted")
        case "rejected": return qsTr("Rejected")
        case "unknown": return qsTr("Unknown")
        default: return String(state || "")
        }
    }
    function folderTitle() {
        if (draftsVisible) return qsTr("Drafts")
        const folders = mail.folders || []
        for (let i = 0; i < folders.length; ++i)
            if (folders[i].id === mail.currentFolder) return mailboxTitle(folders[i])
        return mail.currentFolder || qsTr("Inbox")
    }
    function dateLabel(date) {
        if (date === "Yesterday") return qsTr("Yesterday")
        const d = new Date(date)
        if (isNaN(d.getTime())) return String(date || "")
        return AppTheme.formatDateTime(d, Locale.ShortFormat)
    }
    function compose(mode) {
        if (mail.sending) return
        composer.editDraft(mail.newDraft(mode || "new"))
    }
    // Capture helper is deliberately unavailable for real accounts.
    function showReview(scenario) {
        if (!mail.demo) return
        moreMenu.close(); listOptions.close(); window.detailsVisible = false; accountDialog.close()
        if (composer.visible && !composer.finishEditing()) return
        draftsVisible = false
        if (scenario === "compose") {
            const sample = mail.newDraft("new")
            if (!sample || !sample.id) return
            sample.to = "studio@example.invalid"
            sample.subject = qsTr("A few thoughts for Friday")
            sample.body = qsTr("Hi Maya,\n\nI like the quieter direction. A little more space to read, and fewer things competing for attention.\n\nLet's bring these ideas to our next review.\n\nTalk soon,\nAlex\n\n—\nSynthetic demo draft. Sending is disabled.")
            composer.editDraft(mail.saveDraft(sample))
        }
        else if (scenario === "account") accountDialog.editAccount()
        else if (scenario === "details" && hasMessage) window.detailsVisible = true
        else if (scenario === "search") readerHeader.openSearch()
        else if (scenario === "options") { readerVisible = false; Qt.callLater(() => listOptions.open()) }
        else if (scenario === "unread") { mail.unreadOnly = true; readerVisible = false }
        else if (scenario === "list") readerVisible = false
        else if (scenario === "reader") readerVisible = hasMessage
    }
    function toggleMaximized() {
        if (visibility === Window.Maximized) showNormal()
        else showMaximized()
    }
    Component.onCompleted: {
        AppTheme.compactDesign = true
        mail.sendDelay = 8
        if (typeof reviewAppearance !== "undefined" && (reviewAppearance === "light" || reviewAppearance === "dark"))
            AppTheme.appearanceMode = reviewAppearance
    }
    onClosing: close => {
        if (composer.visible && (mail.sending || !composer.persist())) {
            close.accepted = false
            composer.forceActiveFocus()
        }
    }
    Connections {
        target: mail
        function onSelectionChanged() { if (window.narrow && window.hasMessage) window.readerVisible = true }
        function onFolderChanged() { window.clearSelection() }
        function onAccountChanged() {
            // switchAccount() already empties the message model; the per-row menus
            // live inside those delegates and are torn down with them.
            window.selectionIndices = ({})
            window.selectionAnchor = -1
            window.collapsedThreads = ({})
            window.detailsVisible = false
            listOptions.close()
        }
    }
    Connections {
        target: mail.messages
        function onModelReset() { window.clearSelection() }
    }
    Item {
        id: mailScene
        objectName: "mailScene"
        anchors.fill: parent

        // Calendar's shell contract: the window is transparent, while these
        // two flush planes own the actual sidebar/content surfaces. Controls
        // are never included in the capture source.
        Item {
            id: chromePlanes
            objectName: "chromePlanes"
            anchors.fill: parent
            z: 0

            Rectangle {
                id: sidebarPlane
                objectName: "sidebarPlane"
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: window.sidebarVisible ? window.sidebarWidth : 0
                color: window.sidebarColor
                opacity: window.liquidControls ? 0.95 : 1
            }

            Rectangle {
                id: contentPlane
                objectName: "contentPlane"
                anchors.left: sidebarPlane.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: AppTheme.contentBackground

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: sidebarPlane.width > 0 ? 1 : 0
                    color: window.rule
                }
            }
        }

        LiquidBackdrop {
            id: chromeBackdrop
            objectName: "chromeBackdrop"
            sourceItem: chromePlanes
            registerAsDefault: false
            active: window.liquidControls
        }

        // The header owns every top-level action; none of the body panes has
        // its own toolbar. Interactive children sit above the drag background.
        Item {
            id: floatingHeader
            objectName: "floatingHeader"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: MailStyle.shellHeaderHeight
            z: 3

            WindowChrome {
                id: windowChrome
                anchors.fill: parent
                targetWindow: window
            }

            Item {
                id: sidebarHeader
                objectName: "sidebarHeader"
                visible: window.sidebarVisible
                width: sidebarPlane.width
                height: parent.height
                Text {
                    x: 14; y: 4
                    width: parent.width - 28; height: 26
                    text: qsTr("Mail"); font.pixelSize: 24; font.weight: Font.Bold
                    color: window.ink; elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
                MailAction {
                    anchors.right: parent.right; anchors.rightMargin: 14
                    y: 36
                    iconOnly: true; iconName: "editRounded"; accessibleText: qsTr("New message")
                    floatingAction: true; shellBackdrop: chromeBackdrop
                    objectName: "sidebarComposeButton"
                    enabled: mail.configured || mail.demo
                    onClicked: window.compose("new")
                }
            }

            Item {
                id: listHeader
                objectName: "listHeader"
                visible: messagePane.visible
                x: contentScene.x + messagePane.x
                width: messagePane.width
                height: parent.height
                readonly property real leadingInset: window.sidebarVisible ? 14 : 48
                Text {
                    objectName: "folderTitle"
                    x: listHeader.leadingInset
                    y: windowChrome.controlsCenterY - height / 2
                    width: window.narrow ? readerHeader.titleWidth : Math.max(0, parent.width - x - 14 - (unreadFilterButton.visible ? unreadFilterButton.width + 8 : 0))
                    height: MailStyle.toolbarButtonHeight
                    text: mail.unreadOnly && !window.draftsVisible ? qsTr("%1 · Unread (cached)").arg(window.folderTitle()) : window.folderTitle()
                    color: window.ink; font.pixelSize: 19; font.weight: Font.Bold
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight; textFormat: Text.PlainText
                }
                MailAction {
                    id: unreadFilterButton
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    y: (44 - height) / 2
                    implicitHeight: 38 // MailAction defaults to the compact toolbar height.
                    objectName: "unreadFilterButton"
                    visible: messagePane.visible && !window.draftsVisible
                    stacked: true
                    stackedText: qsTr("筛选")
                    iconName: "filter"
                    checkable: true
                    checked: mail.unreadOnly
                    highlighted: mail.unreadOnly
                    accessibleText: mail.unreadOnly ? qsTr("Show all cached messages") : qsTr("Show unread cached messages")
                    floatingAction: true; shellBackdrop: chromeBackdrop
                    focusPolicy: Qt.TabFocus
                    onClicked: mail.unreadOnly = !mail.unreadOnly
                    onVisibleChanged: { if (!visible && activeFocus) listOptionsButton.forceActiveFocus() }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: window.rule
                }
            }

            Item {
                id: readerHeader
                objectName: "readerHeader"
                visible: reader.visible
                x: contentScene.x + (window.narrow ? 0 : reader.x)
                // 右侧让出窗口控制区，避免 stacked 按钮与红黄绿重叠
                width: (window.narrow ? contentScene.width : reader.width) - windowChrome.controlsReservedWidth
                height: parent.height
                readonly property real leadingInset: window.narrow && !window.sidebarVisible ? 48 : 14
                readonly property real trailingEdge: Math.floor(Math.min(width - 14, windowChrome.controlsLeft - x - 12))
                readonly property real rowWidth: Math.max(0, trailingEdge - leadingInset)
                readonly property real titleWidth: 0
                readonly property real actionLeft: leadingInset + titleWidth + (titleWidth > 0 ? 8 : 0)
                readonly property real actionWidth: Math.max(0, trailingEdge - actionLeft)
                readonly property bool messageActions: (!window.narrow || window.readerVisible) && !window.draftsVisible
                readonly property bool backVisible: false
                readonly property real baselineWidth: window.draftsVisible ? 52 : 60 + (messageActions ? 34 : 0) + (backVisible ? 34 : 0)
                readonly property bool compactActions: !window.draftsVisible && actionWidth < baselineWidth + 60
                readonly property bool showReplyGroup: messageActions && actionWidth >= baselineWidth + 120
                readonly property bool fullReplyGroup: messageActions && actionWidth >= baselineWidth + 146
                readonly property bool directDetails: messageActions && actionWidth >= baselineWidth + 180
                readonly property bool directFilter: !window.draftsVisible && actionWidth >= baselineWidth + 60 + (messageActions ? 120 : 0) + 34
                readonly property bool searching: false
                // 宽度够就把搜索做成常显输入框，不够就退化成搜索按钮
                readonly property bool wideSearch: window.width >= 1180
                property bool searchOpen: false
                onCompactActionsChanged: {
                    if (window.focusWithin(mailboxActions)) listOptionsButton.forceActiveFocus()
                    if (listOptions.visible) listOptions.close()
                }
                onFullReplyGroupChanged: {
                    if (window.focusWithin(replyActions)) moreButton.forceActiveFocus()
                }
                function openSearch() {
                    listOptions.close(); moreMenu.close(); window.detailsVisible = false
                    window.draftsVisible = false
                    window.readerVisible = false
                    if (window.narrow) window.sidebarExpanded = false
                    toolbarSearch.active = true
                    searchFocus.restart()
                }
                function closeSearch() {
                    searchFocus.stop()
                    searchDelay.stop()
                    toolbarSearch.active = false
                    searchField.text = ""
                    mail.search = ""
                    searchField.focus = false
                }
                RowLayout {
                    x: readerHeader.actionLeft
                    y: windowChrome.controlsCenterY - height / 2
                    width: readerHeader.actionWidth
                    height: MailStyle.toolbarButtonHeight; spacing: 0
                    MailAction {
                        Layout.rightMargin: 8
                        objectName: "readerBackButton"
                        visible: readerHeader.backVisible && !readerHeader.searching
                        stacked: true; stackedText: qsTr("返回"); iconName: "back"; accessibleText: qsTr("Back to messages")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        onClicked: window.readerVisible = false
                    }
                    LiquidActionButtonGroup {
                        id: mailboxActions
                        Layout.rightMargin: 8
                        objectName: "mailboxActionGroup"
                        stacked: true
                        visible: !readerHeader.compactActions && !readerHeader.searching && window.width >= 1180
                        buttonSize: MailStyle.toolbarButtonHeight
                        icons: ["refreshRounded", "editRounded"]
                        labels: [qsTr("收取"), qsTr("撰写")]
                        objectNames: ["refreshButton", "headerComposeButton"]
                        enabledActions: [!mail.busy && !window.draftsVisible && (mail.configured || mail.demo), mail.configured || mail.demo]
                        backdrop: chromeBackdrop
                        materialForm: !window.liquidControls
                        onTriggered: index => {
                            if (index === 0) mail.refresh()
                            else window.compose("new")
                        }
                    }
                    LiquidActionButtonGroup {
                        id: messageOpsGroup
                        objectName: "messageOpsGroup"
                        Layout.rightMargin: 8
                        visible: !window.draftsVisible && window.width >= 1100 && window.selectionSize === 0
                        stacked: true
                        buttonSize: 38
                        icons: ["trash", "archive", "flag"]
                        labels: [qsTr("删除"), qsTr("归档"), qsTr("旗标")]
                        enabledActions: [window.hasMessage, window.hasMessage, window.hasMessage]
                        backdrop: chromeBackdrop
                        materialForm: !window.liquidControls
                        onTriggered: index => {
                            const row = window.selectedIndex()
                            if (row < 0) return
                            if (index === 0) mail.deleteMessage(row)
                            else if (index === 1) mail.archiveMessage(row)
                            else mail.toggleFlagged(row)
                        }
                    }
                    Text {
                        objectName: "selectionCountLabel"
                        visible: window.selectionSize > 0
                        text: qsTr("%1 selected").arg(window.selectionSize)
                        color: window.muted; font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                    LiquidActionButtonGroup {
                        id: selectionOpsGroup
                        objectName: "selectionOpsGroup"
                        stacked: true
                        buttonSize: 38
                        visible: !window.draftsVisible && window.selectionSize > 0
                        icons: ["envelope", "flag", "archive", "trash", "folder"]
                        labels: [qsTr("已读"), qsTr("旗标"), qsTr("归档"), qsTr("删除"), qsTr("移动")]
                        backdrop: chromeBackdrop
                        materialForm: !window.liquidControls
                        onTriggered: index => {
                            if (index === 0) mail.setMessagesRead(window.selectedIndexList(), true)
                            else if (index === 1) mail.setMessagesFlagged(window.selectedIndexList(), true)
                            else if (index === 2) {
                                mail.archiveMessages(window.selectedIndexList())
                                window.clearSelection()
                            } else if (index === 3) {
                                mail.deleteMessages(window.selectedIndexList())
                                window.clearSelection()
                            } else if (index === 4) selectionMoveMenu.openAtAnchor()
                        }
                    }
                    LiquidPopover {
                        id: selectionMoveMenu
                        objectName: "selectionMoveMenu"
                        sourceItem: chromePlanes
                        anchorItem: selectionOpsGroup
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                        Repeater {
                            model: mail.folders.filter(f => f.id !== mail.currentFolder)
                            LiquidPopoverItem {
                                text: modelData.title || modelData.id
                                iconName: "folder"
                                onTriggered: {
                                    selectionMoveMenu.close()
                                    mail.moveMessages(window.selectedIndexList(), modelData.id)
                                    window.clearSelection()
                                }
                            }
                        }
                    }
                    LiquidActionButtonGroup {
                        id: replyActions
                        Layout.rightMargin: 8
                        objectName: "replyActionGroup"
                        stacked: true
                        visible: readerHeader.showReplyGroup && !readerHeader.searching && window.width >= 1480
                        buttonSize: MailStyle.toolbarButtonHeight
                        icons: readerHeader.fullReplyGroup ? ["undo", "arrows", "forward"] : ["undo", "forward"]
                        labels: readerHeader.fullReplyGroup ? [qsTr("回复"), qsTr("全部回复"), qsTr("转发")] : [qsTr("回复"), qsTr("转发")]
                        objectNames: readerHeader.fullReplyGroup ? ["replyButton", "replyAllButton", "forwardButton"] : ["replyButton", "forwardButton"]
                        enabledActions: [window.hasMessage && !mail.sending, window.hasMessage && !mail.sending, window.hasMessage && !mail.sending]
                        backdrop: chromeBackdrop
                        materialForm: !window.liquidControls
                        onTriggered: index => window.compose(readerHeader.fullReplyGroup ? ["reply", "replyAll", "forward"][index] : ["reply", "forward"][index])
                        onVisibleChanged: { if (!visible && window.focusWithin(replyActions)) moreButton.forceActiveFocus() }
                    }
                    LiquidActionButtonGroup {
                        id: messageOptionsGroup
                        objectName: "messageOptionsGroup"
                        Layout.rightMargin: 8
                        visible: !window.draftsVisible
                        stacked: true
                        buttonSize: 38
                        icons: readerHeader.wideSearch ? ["sliders", "info", "more"] : ["sliders", "more"]
                        labels: readerHeader.wideSearch ? [qsTr("显示"), qsTr("详情"), qsTr("更多")] : [qsTr("显示"), qsTr("更多")]
                        objectNames: readerHeader.wideSearch ? ["listOptionsButton", "detailsButton", "moreButton"] : ["listOptionsButton", "moreButton"]
                        enabledActions: [true, window.hasMessage, window.hasMessage]
                        backdrop: chromeBackdrop
                        materialForm: !window.liquidControls
                        onTriggered: index => {
                            if (index === 0) listOptions.open()
                            else if (index === 1) window.detailsVisible = true
                            else moreMenu.openAtAnchor()
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
                        property bool active: true
                        readonly property real wanted: 216
                        readonly property bool shown: !window.draftsVisible
                            && (readerHeader.wideSearch || readerHeader.searchOpen)
                            && window.width >= 1320
                        Layout.preferredWidth: shown ? wanted : 0
                        Layout.minimumWidth: 0
                        Layout.fillHeight: true
                        Layout.rightMargin: shown ? 8 : 0
                        visible: shown
                            clip: true
                            LiquidTextField {
                                id: searchField
                                objectName: "searchField"
                                anchors.fill: parent
                                implicitHeight: MailStyle.toolbarButtonHeight
                                enabled: !window.draftsVisible
                                materialForm: !window.liquidControls
                                liquidFinish: true
                                liquidIntensity: "light"
                                backdrop: chromeBackdrop
                                leftPadding: 30
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
                        }
                    MailAction {
                        objectName: "searchToggleButton"
                        Layout.rightMargin: 8
                        visible: !readerHeader.wideSearch && !readerHeader.searchOpen && !window.draftsVisible
                        stacked: true
                        stackedText: qsTr("搜索")
                        iconName: "search"
                        accessibleText: qsTr("Search cached mail")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        onClicked: { readerHeader.searchOpen = true; searchField.focus = true }
                    }
                                    }
                Timer {
                    id: searchFocus
                    interval: AppTheme.reduceMotion ? 0 : AppTheme.motionFast + 50
                    onTriggered: { if (toolbarSearch.active && readerHeader.visible) searchField.forceActiveFocus() }
                }
            }

            Item {
                id: sidebarToggleHost
                x: window.sidebarVisible ? 0 : 14
                width: window.sidebarVisible ? sidebarPlane.width : sidebarToggleButton.width
                height: parent.height
                MailAction {
                    id: sidebarToggleButton
                    objectName: "sidebarToggleButton"
                    anchors.right: parent.right
                    anchors.rightMargin: window.sidebarVisible ? 12 : 0
                    y: windowChrome.controlsCenterY - height / 2
                    stacked: true; stackedText: qsTr("侧栏"); iconName: "panelLeft"
                    accessibleText: window.sidebarVisible ? qsTr("Hide sidebar") : qsTr("Show sidebar")
                    floatingAction: true; shellBackdrop: chromeBackdrop
                    liquidIntensity: "strong"
                    focusRingColor: materialForm ? AppTheme.focusRing : "transparent"
                    focusPolicy: Qt.TabFocus
                    onClicked: window.toggleSidebar()
                }
            }
        }

        // Gallery navigation owns row visuals; NkMail adds only mailbox counts.
        component MailboxAction: LiquidNavigationButton {
            id: action
            property string countText: ""
            property string unreadText: ""
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            specificationForm: true
            specificationFontSize: 12
            specificationSelectedVerticalInset: 2
            specificationSelectedFillColor: AppTheme.dark ? Qt.rgba(0.31, 0.56, 0.96, 0.28) : "#DCE9FB"
            specificationSelectedTextColor: AppTheme.dark ? "#EAF1FF" : AppTheme.accent
            specificationHoverFillColor: AppTheme.dark ? Qt.rgba(1, 1, 1, .08) : "#dbdbdd"
            font.weight: Font.Medium
            checkable: false
            checked: false
            iconColor: checked ? specificationSelectedTextColor : AppTheme.mutedText
            contentItem: Item {
                implicitWidth: caption.implicitWidth
                    + (action.iconName.length ? 24 : 0)
                    + (unread.visible ? unread.implicitWidth + 6 : 0)
                implicitHeight: Math.max(caption.implicitHeight, 16)
                VectorIcon {
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    visible: action.iconName.length > 0
                    name: action.iconName; size: 14; color: action.iconColor
                }
                Text {
                    id: caption
                    anchors.left: parent.left; anchors.right: unread.visible ? unread.left : parent.right
                    anchors.leftMargin: action.iconName.length ? 24 : 0
                    anchors.rightMargin: unread.visible ? 6 : 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: action.text
                    color: action.checked ? action.specificationSelectedTextColor
                        : AppTheme.dark ? Qt.rgba(1, 1, 1, .92) : Qt.rgba(0, 0, 0, .92)
                    font: action.font
                    verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                }
                Text {
                    id: unread
                    objectName: "mailboxUnread"
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    visible: text.length > 0
                    text: action.unreadText
                    font: action.font
                    color: action.checked ? action.specificationSelectedTextColor : AppTheme.accent
                    textFormat: Text.PlainText
                }
                Text {
                    id: count
                    objectName: "mailboxCount"
                    visible: false
                    text: action.countText
                    font: action.font
                    textFormat: Text.PlainText
                }
            }
        }

        // Sidebar account row: a disclosure chevron plus the account title. The
        // title colour distinguishes the active account from the others.
        component AccountHeader: LiquidNavigationButton {
            id: header
            property string accountId: ""
            property bool current: false
            property bool collapsed: false
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            specificationForm: true
            specificationFontSize: 12
            specificationSelectedVerticalInset: 2
            specificationHoverFillColor: AppTheme.dark ? Qt.rgba(1, 1, 1, .08) : "#dbdbdd"
            checkable: false
            checked: false
            contentItem: Item {
                implicitHeight: Math.max(title.implicitHeight, 16)
                VectorIcon {
                    id: chevron
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    name: "chevron"; size: 12
                    color: header.current ? AppTheme.text : AppTheme.mutedText
                    rotation: header.collapsed ? -90 : 0
                    Behavior on rotation {
                        NumberAnimation { duration: AppTheme.reduceMotion ? 0 : AppTheme.motionFast }
                    }
                }
                Text {
                    id: title
                    anchors.fill: parent; anchors.leftMargin: 18
                    text: header.text
                    color: header.current ? AppTheme.text : AppTheme.mutedText
                    font.pixelSize: 13; font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                }
                Text {
                    id: accountUnread
                    objectName: "accountUnread_" + header.accountId
                    visible: false
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    text: {
                        window.unreadRevision
                        window.mail.accountUnreadRevision
                        const count = window.mail.accountUnread(header.accountId)
                        return count > 0 ? String(count) : ""
                    }
                    color: AppTheme.mutedText; font.pixelSize: 11
                    textFormat: Text.PlainText
                }
            }
        }

        Item {
                id: sidebar
                objectName: "sidebarBody"
                visible: window.sidebarVisible
                anchors.left: parent.left
                anchors.top: floatingHeader.bottom
                anchors.bottom: statusStrip.top
                width: sidebarPlane.width
                z: 1
                Flickable {
                    id: mailboxNavigation
                    objectName: "mailboxNavigation"
                    anchors.fill: parent
                    anchors.leftMargin: 14; anchors.rightMargin: 14
                    anchors.bottomMargin: 14; anchors.topMargin: 4
                    clip: true
                    contentWidth: width
                    contentHeight: mailboxColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    ColumnLayout {
                        id: mailboxColumn
                        width: parent.width
                        spacing: 0
                        Text {
                            objectName: "favoritesTitle"
                            visible: (mail.favorites || []).length > 0
                            text: qsTr("收藏"); color: window.muted
                            font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 0.4
                            topPadding: 0; bottomPadding: 6
                        }
                        Repeater {
                            objectName: "favoritesList"
                            model: mail.favorites || []
                            delegate: MailboxAction {
                                required property var modelData
                                text: modelData.title
                                iconName: window.folderIcon(modelData.folderId)
                                countText: ""
                                onClicked: mail.openFavorite(modelData.accountId, modelData.folderId)
                            }
                        }
                        Repeater {
                            model: window.sidebarAccounts
                            delegate: ColumnLayout {
                                id: accountSection
                                required property var modelData
                                readonly property string accountId: modelData.id
                                readonly property bool current: window.mail.currentAccountId === accountId
                                readonly property bool collapsed: window.collapsedAccounts[accountId] === true
                                Layout.fillWidth: true
                                spacing: 0
                                AccountHeader {
                                    id: accountRow
                                    objectName: "accountHeader_" + accountSection.accountId
                                    accountId: accountSection.accountId
                                    current: accountSection.current
                                    collapsed: accountSection.collapsed
                                    text: accountSection.modelData.name || accountSection.modelData.address
                                    Accessible.name: text
                                    onClicked: {
                                        if (accountSection.current) {
                                            window.toggleAccountCollapsed(accountSection.accountId)
                                        } else {
                                            window.mail.switchAccount(accountSection.accountId)
                                            if (accountSection.collapsed)
                                                window.toggleAccountCollapsed(accountSection.accountId)
                                        }
                                    }
                                    TapHandler {
                                        acceptedButtons: Qt.RightButton
                                        onTapped: accountMenu.openAtAnchor()
                                    }
                                    LiquidPopover {
                                        id: accountMenu
                                        objectName: "accountMenu"
                                        sourceItem: chromePlanes
                                        anchorItem: accountRow
                                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                                        LiquidPopoverItem {
                                            text: qsTr("编辑账号"); iconName: "settings"
                                            onTriggered: {
                                                accountMenu.close()
                                                accountDialog.editAccount(accountSection.modelData)
                                            }
                                        }
                                        LiquidPopoverItem {
                                            text: qsTr("删除账号"); iconName: "trash"
                                            onTriggered: {
                                                accountMenu.close()
                                                deleteAccountDialog.targetId = accountSection.accountId
                                                deleteAccountDialog.targetName = accountSection.modelData.name
                                                    || accountSection.modelData.address || ""
                                                deleteAccountDialog.open()
                                            }
                                        }
                                    }
                                }
                                Repeater {
                                    // Synchronous cache read; it is not re-evaluated when
                                    // the active account changes, only when the section folds.
                                    objectName: accountSection.current ? "mailboxFolders" : "mailboxFolders_" + accountSection.accountId
                                    model: accountSection.collapsed ? [] : (window.mail.foldersFor(accountSection.accountId) || [])
                                    delegate: MailboxAction {
                                        id: folderRow
                                        required property var modelData
                                        // Keep the historical name for the active account; scope
                                        // other accounts so their identical folder ids stay unique.
                                        objectName: accountSection.current
                                            ? "mailboxFolder_" + modelData.id
                                            : "mailboxFolder_" + accountSection.accountId + "_" + modelData.id
                                        text: window.mailboxTitle(modelData)
                                        iconName: window.folderIcon(modelData.id)
                                        countText: ""
                                        unreadText: {
                                            window.unreadRevision
                                            const count = accountSection.current
                                                ? (window.mail.folderUnread()[modelData.id] || 0) : 0
                                            return count > 0 ? String(count) : ""
                                        }
                                        checked: accountSection.current && !window.draftsVisible && window.mail.currentFolder === modelData.id
                                        Accessible.name: window.mailboxTitle(modelData)
                                        onClicked: {
                                            if (!accountSection.current) {
                                                window.mail.switchAccount(accountSection.accountId)
                                                if (window.mail.currentAccountId !== accountSection.accountId) return
                                            }
                                            window.draftsVisible = false; window.readerVisible = false
                                            window.mail.selectFolder(modelData.id)
                                            if (window.width < 1080) window.sidebarExpanded = false
                                        }
                                        TapHandler {
                                            acceptedButtons: Qt.RightButton
                                            onTapped: folderMenu.openAtAnchor()
                                        }
                                        LiquidPopover {
                                            id: folderMenu
                                            objectName: "folderMenu"
                                            sourceItem: chromePlanes
                                            anchorItem: folderRow
                                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                                            LiquidPopoverItem {
                                                // Touching mail.favorites keeps the label in sync when
                                                // the list changes; isFavorite() itself is not tracked.
                                                text: (mail.favorites, mail.isFavorite(accountSection.accountId, folderRow.modelData.id))
                                                    ? qsTr("从收藏移除") : qsTr("添加到收藏")
                                                iconName: "heart"
                                                onTriggered: {
                                                    mail.toggleFavorite(accountSection.accountId, folderRow.modelData.id,
                                                        window.mailboxTitle(folderRow.modelData))
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        MailboxAction {
                            objectName: "draftFolderButton"
                            text: qsTr("Drafts")
                            iconName: "editRounded"
                            checked: window.draftsVisible
                            Accessible.name: qsTr("Local drafts")
                            onClicked: { window.draftsVisible = true; window.readerVisible = false; if (window.width < 1080) window.sidebarExpanded = false }
                        }
                        Item {
                            id: addAccountRow
                            objectName: "addAccountButton"
                            Accessible.name: qsTr("添加账号")
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            Layout.topMargin: 6
                            readonly property bool hovered: addAccountMouse.containsMouse
                            Rectangle {
                                anchors.fill: parent; radius: 8
                                color: addAccountRow.hovered
                                    ? (AppTheme.dark ? Qt.rgba(1, 1, 1, .08) : "#dbdbdd") : "transparent"
                                Behavior on color {
                                    ColorAnimation { duration: AppTheme.reduceMotion ? 0 : AppTheme.motionFast }
                                }
                            }
                            Item {
                                anchors.left: parent.left; anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: addIcon.implicitWidth + 5 + addLabel.implicitWidth
                                height: 16
                                VectorIcon {
                                    id: addIcon
                                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                                    name: "plus"; size: 13
                                    color: addAccountRow.hovered ? AppTheme.text : AppTheme.mutedText
                                }
                                Text {
                                    id: addLabel
                                    anchors.left: addIcon.right; anchors.leftMargin: 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("添加账号")
                                    color: addAccountRow.hovered ? AppTheme.text : AppTheme.mutedText
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            MouseArea {
                                id: addAccountMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: accountDialog.editAccount()
                            }
                        }
                    }
                }
        }

        RowLayout {
            id: contentScene
            objectName: "contentScene"
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: floatingHeader.bottom
            anchors.bottom: statusStrip.top
            spacing: 0
            z: 1

            Rectangle {
                id: messagePane
                objectName: "messagePane"
                visible: !window.narrow || !window.readerVisible
                Layout.preferredWidth: window.listWidth
                Layout.minimumWidth: window.narrow ? 260 : window.listWidth
                Layout.maximumWidth: window.narrow ? -1 : window.listWidth
                Layout.fillWidth: window.narrow
                Layout.fillHeight: true
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    ListView {
                        id: messageList
                        objectName: "messageList"
                        visible: !window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; boundsBehavior: Flickable.StopAtBounds
                        model: mail.messages
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: AbstractButton {
                            id: messageRow
                            required property int index
                            required property string uid
                            required property string uidValidity
                            required property string sender
                            required property string senderAddress
                            required property string subject
                            required property var date
                            required property string preview
                            required property bool unread
                            required property bool flagged
                            required property bool hasAttachments
                            required property string threadKey
                            required property int threadCount
                            required property bool threadFirst
                            readonly property bool hiddenByThread: window.groupThreads && !threadFirst && window.collapsedThreads[threadKey] === true
                            width: ListView.view.width
                            visible: !hiddenByThread
                            height: visible ? MailStyle.messageRowHeight : 0
                            implicitHeight: visible ? MailStyle.messageRowHeight : 0
                            hoverEnabled: true
                            readonly property bool selected: mail.selectedUid === uid
                            readonly property bool multiSelected: window.isRowSelected(index)
                            readonly property int messageIndex: (mail.messages && typeof mail.messages.indexOf === "function")
                                ? mail.messages.indexOf(mail.currentFolder, uidValidity, uid) : -1
                            function selectRow() { mail.openMessage(uid, uidValidity); window.readerVisible = true }
                            function openRow() {
                                selectRow()
                                if (unread) mail.setMessageRead(messageIndex, true)
                            }
                            Accessible.name: sender + ". " + subject + (window.showSummary ? ". " + preview : "")
                            onClicked: {}   // 鼠标点击由 TapHandler 处理；键盘见下方 Keys
                            Keys.onReturnPressed: openRow()
                            Keys.onEnterPressed: openRow()
                            Keys.onSpacePressed: openRow()
                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onTapped: function(point) {
                                    if (point.modifiers & Qt.ShiftModifier) window.selectRangeTo(index)
                                    else if (point.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) window.toggleRow(index)
                                    else { window.clearSelection(); messageRow.openRow() }
                                }
                            }
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: { messageRow.selectRow(); rowMenu.openAtAnchor() }
                            }
                            LiquidPopover {
                                id: rowMenu
                                objectName: "messageRowMenu"
                                sourceItem: chromePlanes
                                anchorItem: messageRow
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                                LiquidPopoverItem { text: qsTr("打开"); iconName: "reminder"; onTriggered: messageRow.openRow() }
                                LiquidPopoverItem {
                                    text: messageRow.unread ? qsTr("标为已读") : qsTr("标为未读")
                                    iconName: messageRow.unread ? "envelopeOpen" : "envelope"
                                    onTriggered: mail.setMessageRead(messageRow.messageIndex, messageRow.unread)
                                }
                                LiquidPopoverItem {
                                    text: messageRow.flagged ? qsTr("移除旗标") : qsTr("添加旗标")
                                    iconName: "flag"
                                    onTriggered: mail.toggleFlagged(messageRow.messageIndex)
                                }
                                LiquidPopoverItem {
                                    objectName: "moveToMenuItem"
                                    text: qsTr("移动到…")
                                    iconName: "folder"
                                    enabled: mail.folders.filter(f => f.id !== mail.currentFolder).length > 0
                                    onTriggered: { rowMenu.close(); messageMoveMenu.openAtAnchor() }
                                }
                                LiquidPopoverItem { text: qsTr("归档"); iconName: "archive"; onTriggered: mail.archiveMessage(messageRow.messageIndex) }
                                LiquidPopoverItem { text: qsTr("删除"); iconName: "trash"; onTriggered: mail.deleteMessage(messageRow.messageIndex) }
                            }
                            LiquidPopover {
                                id: messageMoveMenu
                                objectName: "messageMoveMenu"
                                sourceItem: chromePlanes
                                anchorItem: messageRow
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                                Repeater {
                                    model: mail.folders.filter(f => f.id !== mail.currentFolder)
                                    LiquidPopoverItem {
                                        text: modelData.title || modelData.id
                                        iconName: "folder"
                                        onTriggered: {
                                            messageMoveMenu.close()
                                            mail.moveMessage(messageRow.messageIndex, modelData.id)
                                        }
                                    }
                                }
                            }
                            background: Rectangle {
                                color: messageRow.multiSelected ? window.selectedSurface : messageRow.selected ? window.selectedSurface : messageRow.hovered ? window.hoverSurface : "transparent"
                                border.width: messageRow.activeFocus ? 1 : 0
                                border.color: AppTheme.focusRing
                                Rectangle { height: 1; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.leftMargin: 19; anchors.right: parent.right; color: window.rule }
                            }
                            contentItem: Item {
                                Rectangle { x: 9; y: 23; width: 6; height: 6; radius: 3; color: AppTheme.accent; visible: messageRow.unread }
                                ColumnLayout {
                                    anchors.fill: parent; anchors.leftMargin: 23; anchors.rightMargin: 17; anchors.topMargin: 14; anchors.bottomMargin: 13
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: messageRow.sender || messageRow.senderAddress; color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                        Rectangle {
                                            id: threadBadge
                                            visible: window.groupThreads && messageRow.threadFirst && messageRow.threadCount > 1
                                            implicitWidth: threadBadgeRow.implicitWidth + 12
                                            implicitHeight: 18
                                            radius: 9
                                            color: window.hoverSurface
                                            border.width: 1
                                            border.color: window.rule
                                            RowLayout {
                                                id: threadBadgeRow
                                                anchors.centerIn: parent
                                                spacing: 3
                                                Text { text: messageRow.threadCount; color: window.muted; font.pixelSize: 10 }
                                                VectorIcon { name: "chevronsUpDown"; size: 12; color: window.muted }
                                            }
                                            TapHandler {
                                                grabPermissions: PointerHandler.CanTakeOverFromAnything
                                                onTapped: window.toggleThread(messageRow.threadKey)
                                            }
                                        }
                                        Text { text: window.dateLabel(messageRow.date); color: window.muted; font.pixelSize: AppTheme.compactControlFontSize }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text { text: messageRow.subject || qsTr("(No subject)"); color: window.ink; font.pixelSize: 12; font.weight: messageRow.unread ? Font.DemiBold : Font.Normal; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                        Text { text: messageRow.flagged ? "★" : ""; color: AppTheme.accent; font.pixelSize: 12; visible: messageRow.flagged; Accessible.name: qsTr("Flagged") }
                                        VectorIcon { name: "album"; size: 14; color: window.muted; visible: messageRow.hasAttachments; Accessible.name: qsTr("Has attachment") }
                                    }
                                    Text { objectName: "messagePreview"; visible: window.showSummary; text: messageRow.preview; color: window.muted; font.pixelSize: 11; lineHeight: 1.25; Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                                }
                            }
                        }
                        footer: Item {
                            width: messageList.width; height: mail.hasMore ? 48 : 0
                            MailAction { anchors.centerIn: parent; visible: mail.hasMore; text: qsTr("Load older messages"); enabled: !mail.busy; onClicked: mail.loadMore() }
                        }
                        KosEmptyState {
                            anchors.centerIn: parent
                            width: parent.width - 40
                            visible: messageList.count === 0
                            symbol: mail.busy ? "…" : mail.search ? "⌕" : "✉"
                            objectName: "messageEmptyState"
                            title: mail.busy ? qsTr("Getting your mail…") : mail.search ? qsTr("No cached matches") : mail.unreadOnly ? qsTr("No unread messages in cached mail") : mail.configured || mail.demo ? qsTr("No messages here") : qsTr("Welcome to your new inbox")
                            description: mail.busy ? "" : mail.unreadOnly ? qsTr("Only cached unread messages are shown. Clear the filter to browse all messages.") : mail.search ? qsTr("Try a different search term.") : mail.configured || mail.demo ? qsTr("There is nothing to show in this mailbox yet.") : qsTr("Add an account to start reading mail locally.")
                            actionText: mail.unreadOnly ? qsTr("Show all cached messages") : !mail.configured && !mail.demo ? qsTr("Add an account") : ""
                            onActionTriggered: {
                                if (mail.unreadOnly) mail.unreadOnly = false
                                else accountDialog.editAccount()
                            }
                        }
                    }
                    ListView {
                        visible: window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        model: mail.drafts; clip: true
                        delegate: AbstractButton {
                            id: draftRow
                            required property var modelData
                            width: ListView.view.width; height: 86
                            Accessible.name: window.value(modelData, "subject") || qsTr("Untitled draft")
                            onClicked: composer.editDraft(mail.loadDraft(modelData.id))
                            background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; height: 1; width: parent.width; color: window.rule } }
                            contentItem: ColumnLayout {
                                spacing: 5
                                Text { text: window.value(draftRow.modelData, "subject") || qsTr("Untitled draft"); color: window.ink; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                Text { text: window.value(draftRow.modelData, "to") || qsTr("No recipient"); color: window.muted; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                Text { text: window.draftStateLabel(draftRow.modelData.state); color: window.muted; font.pixelSize: 10 }
                            }
                            leftPadding: 22; rightPadding: 18; topPadding: 14; bottomPadding: 14
                        }
                        Text { anchors.centerIn: parent; visible: mail.drafts.length === 0; text: qsTr("Your drafts will be saved here."); color: window.muted; font.pixelSize: 12 }
                    }
                }
            }

            Rectangle {
                id: reader
                objectName: "messageReader"
                visible: !window.narrow || window.readerVisible
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    ScrollView {
                        id: readerScroll
                        objectName: "readerScroll"
                        visible: window.hasMessage && !window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentWidth: availableWidth; clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            width: readerScroll.availableWidth
                            spacing: 0
                            ColumnLayout {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: false
                                Layout.preferredWidth: Math.max(0, Math.min(MailStyle.readerMaximumWidth, readerScroll.availableWidth - (window.width < 1100 ? 48 : 72)))
                                Layout.maximumWidth: Math.max(0, Math.min(MailStyle.readerMaximumWidth, readerScroll.availableWidth - (window.width < 1100 ? 48 : 72)))
                                Layout.minimumWidth: 0
                                spacing: 14
                                Item {
                                    id: detailsPanel
                                    objectName: "messageDetailsPanel"
                                    Layout.fillWidth: true
                                    readonly property real contentHeight: Math.max(140, Math.min(380, detailsColumn.implicitHeight + 40))
                                    visible: window.detailsVisible
                                    implicitHeight: visible ? contentHeight : 0
                                    Layout.preferredHeight: implicitHeight
                                    clip: true
                                    RowLayout {
                                        id: detailsHeader
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        anchors.topMargin: 12
                                        height: 28
                                        spacing: 8
                                        Text { text: qsTr("Message details"); color: window.ink; font.pixelSize: 15; font.weight: Font.DemiBold; Layout.fillWidth: true }
                                        MailAction { stacked: true; stackedText: qsTr("关闭"); iconName: "close"; accessibleText: qsTr("Close message details"); onClicked: window.detailsVisible = false }
                                    }
                                    ScrollView {
                                        id: detailsScroll
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: detailsHeader.bottom
                                        anchors.bottom: parent.bottom
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        anchors.topMargin: 8
                                        anchors.bottomMargin: 12
                                        contentWidth: availableWidth
                                        clip: true
                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                        ColumnLayout {
                                            id: detailsColumn
                                            width: parent.width
                                            spacing: 8
                                            Repeater {
                                                model: [ {title: qsTr("From"), value: window.value(window.message, "senderAddress")},
                                                    {title: qsTr("To"), value: window.value(window.message, "to")},
                                                    {title: qsTr("Cc"), value: window.value(window.message, "cc")},
                                                    {title: qsTr("Date"), value: window.dateLabel(window.message.date)},
                                                    {title: qsTr("Message ID"), value: window.value(window.message, "messageId")} ]
                                                delegate: ColumnLayout {
                                                    id: detailsEntry
                                                    required property var modelData
                                                    visible: modelData.value.length > 0
                                                    Layout.fillWidth: true; spacing: 3
                                                    Text { text: detailsEntry.modelData.title; color: window.muted; font.pixelSize: 11 }
                                                    TextEdit { text: detailsEntry.modelData.value; color: window.ink; font.pixelSize: 13; readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true }
                                                }
                                            }
                                            Text { text: qsTr("Read-only mailbox · no server flags are changed."); color: window.muted; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
                                        }
                                    }
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 1
                                        color: window.rule
                                    }
                                }
                                Text {
                                    text: window.value(window.message, "subject") || qsTr("(No subject)")
                                    color: window.ink; font.pixelSize: 25; font.weight: Font.Bold
                                    wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText
                                }
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 12
                                    Rectangle {
                                        width: 36; height: 36; radius: 18; color: MailStyle.avatar
                                        Text { anchors.centerIn: parent; text: (window.value(window.message, "sender") || "?").slice(0, 1).toUpperCase(); color: AppTheme.accent; font.pixelSize: 16; font.weight: Font.DemiBold }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Text { text: window.value(window.message, "sender") || window.value(window.message, "senderAddress"); color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                        Text { text: qsTr("To: ") + window.value(window.message, "to"); color: window.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                    }
                                }
                                Text { text: window.dateLabel(window.message.date); color: window.muted; font.pixelSize: 11; Layout.fillWidth: true }
                                Rectangle { Layout.fillWidth: true; height: 1; color: window.rule; Layout.topMargin: 4; Layout.bottomMargin: 6 }
                                RowLayout {
                                    objectName: "blockedImagesBar"
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: window.mail.blockedImageCount > 0
                                    Text {
                                        text: qsTr("%1 张远程图片已隐藏").arg(window.mail.blockedImageCount)
                                        color: window.muted; font.pixelSize: 11
                                        Layout.fillWidth: true
                                    }
                                    MailAction {
                                        stacked: true; stackedText: qsTr("显示图片"); iconName: "download"
                                        accessibleText: qsTr("Load remote images")
                                        onClicked: window.mail.loadRemoteImages()
                                    }
                                }
                                // Render only the backend-sanitized html field; fall back to plain text.
                                TextEdit {
                                    id: messageBody
                                    objectName: "messageBody"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(200, contentHeight)
                                    readonly property string richHtml: window.value(window.message, "html") || ""
                                    readonly property bool asRich: window.showRichText && String(richHtml).length > 0
                                    text: asRich ? String(richHtml) : window.value(window.message, "text")
                                    textFormat: asRich ? TextEdit.RichText : TextEdit.PlainText
                                    readOnly: true; selectByMouse: true
                                    wrapMode: TextEdit.Wrap
                                    color: window.ink
                                    selectionColor: AppTheme.accent; selectedTextColor: "white"
                                    font.pixelSize: 14
                                    onLinkActivated: function(link) {
                                        if (!/^(https?:|mailto:)/i.test(String(link))) return
                                        linkClip.text = String(link)
                                        linkClip.selectAll(); linkClip.copy()
                                        mail.clearError()
                                        statusHint.show(qsTr("Link copied to clipboard"))
                                    }
                                }
                                TextEdit { id: linkClip; visible: false }
                                Text {
                                    id: statusHint
                                    visible: false
                                    text: ""
                                    color: window.muted; font.pixelSize: 11
                                    function show(message) { text = message; visible = true; hintTimer.restart() }
                                    Timer { id: hintTimer; interval: 2500; onTriggered: statusHint.visible = false }
                                }
                                Repeater {
                                    model: window.message.attachments || []
                                    delegate: Rectangle {
                                        id: attachmentRow
                                        required property var modelData
                                        required property int index
                                        Layout.fillWidth: true; implicitHeight: 48
                                        radius: 6; color: AppTheme.contentSecondaryBackground
                                        RowLayout {
                                            anchors.fill: parent; anchors.margins: 10
                                            VectorIcon { name: "album"; size: 18; color: window.muted }
                                            Text { text: attachmentRow.modelData.name; color: window.ink; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                            MailAction { stacked: true; stackedText: qsTr("保存"); iconName: "download"; accessibleText: qsTr("Save attachment"); onClicked: { saveAttachmentDialog.attachmentIndex = attachmentRow.index; saveAttachmentDialog.open() } }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Item {
                        visible: !window.hasMessage || window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Column {
                            anchors.centerIn: parent; spacing: 12; width: Math.min(310, parent.width - 48)
                            Text { width: parent.width; text: window.draftsVisible ? qsTr("Pick up where you left off.") : qsTr("A moment of quiet."); color: window.muted; font.pixelSize: 22; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                            Text { width: parent.width; text: window.draftsVisible ? qsTr("Choose a draft to continue writing.") : qsTr("Select a message to read it here."); color: window.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                            MailAction { anchors.horizontalCenter: parent.horizontalCenter; visible: !mail.configured && !mail.demo; text: qsTr("Add an account"); onClicked: accountDialog.editAccount() }
                        }
                    }
                }
            }
        }
        Rectangle {
            objectName: "messageColumnRule"
            x: contentScene.x + messagePane.x + messagePane.width - width
            y: 0
            width: 1
            height: statusStrip.y - y
            color: window.rule
            visible: !window.narrow
            z: 2
        }

        // Generous invisible hit targets keep the one-pixel column rules quiet.
        // Focused dividers are also adjustable with the arrow keys.
        component ColumnDivider: MouseArea {
            id: divider
            required property bool sidebarHandle
            property real pressSceneX: 0
            property int startWidth: 0
            width: 8
            y: floatingHeader.height
            height: statusStrip.y - y
            z: 4
            cursorShape: Qt.SplitHCursor
            hoverEnabled: true
            activeFocusOnTab: true
            preventStealing: true
            Accessible.role: Accessible.Slider
            Accessible.name: sidebarHandle ? qsTr("Mailbox column width") : qsTr("Message column width")
            Accessible.description: qsTr("Drag or use Left and Right to resize. Double-click to reset.")
            function resizeTo(value) {
                if (sidebarHandle) window.sidebarPreferredWidth = Math.round(Math.max(190, Math.min(340, value, window.width - 560)))
                else window.listPreferredWidth = Math.round(Math.max(260, Math.min(520, value,
                    window.width - (window.sidebarVisible ? window.sidebarWidth : 0) - 300)))
            }
            onPressed: mouse => {
                pressSceneX = mapToItem(mailScene, mouse.x, mouse.y).x
                startWidth = sidebarHandle ? window.sidebarWidth : window.listWidth
                forceActiveFocus()
            }
            onPositionChanged: mouse => {
                if (pressed) resizeTo(startWidth + mapToItem(mailScene, mouse.x, mouse.y).x - pressSceneX)
            }
            onDoubleClicked: {
                if (sidebarHandle) window.sidebarPreferredWidth = 236
                else window.listPreferredWidth = 320
            }
            Keys.onLeftPressed: resizeTo((sidebarHandle ? window.sidebarWidth : window.listWidth) - 10)
            Keys.onRightPressed: resizeTo((sidebarHandle ? window.sidebarWidth : window.listWidth) + 10)
            Rectangle {
                anchors.centerIn: parent; width: 2; height: parent.height
                color: AppTheme.accent
                opacity: divider.pressed || divider.activeFocus ? 0.8 : divider.containsMouse ? 0.35 : 0
            }
        }
        ColumnDivider {
            objectName: "sidebarDivider"
            sidebarHandle: true
            visible: window.sidebarVisible && !window.narrow
            x: sidebar.width - width / 2
        }
        ColumnDivider {
            objectName: "messageDivider"
            sidebarHandle: false
            visible: !window.narrow
            x: contentScene.x + messagePane.x + messagePane.width - width / 2
        }

        Rectangle {
            id: statusStrip
            objectName: "statusStrip"
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: mail.error ? 44 : 29
            color: mail.error ? (AppTheme.dark ? "#452f30" : "#fff1ef") : window.sidebarColor
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: window.rule }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 14; spacing: 10
                Rectangle { width: 5; height: 5; radius: 3; color: mail.error ? AppTheme.destructive : mail.busy ? AppTheme.accent : window.muted }
                Text {
                    text: mail.error || (mail.demo ? qsTr("Demo · synthetic mail only · sending disabled") : mail.status || qsTr("Ready"))
                    color: mail.error ? AppTheme.destructive : window.muted; font.pixelSize: 10
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
                MailAction { visible: !!mail.error; implicitHeight: 24; text: qsTr("Dismiss"); onClicked: mail.clearError() }
                MailAction { implicitHeight: 24; iconOnly: true; iconName: "theme"; accessibleText: qsTr("Switch light / dark appearance"); onClicked: AppTheme.appearanceMode = AppTheme.dark ? "light" : "dark" }
            }
        }

    }

    KosWindowResizeHandles { targetWindow: window }

    LiquidContentPopover {
        id: listOptions
        objectName: "listOptionsPopover"
        sourceItem: chromePlanes
        anchorItem: messageOptionsGroup
        preferredSide: "bottom"
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        onOpened: unreadFilterCheck.forceActiveFocus()
        width: Math.min(300, window.width - 16)
        implicitHeight: listOptionsColumn.implicitHeight + topPadding + bottomPadding
        height: implicitHeight
        contentItem: ColumnLayout {
            id: listOptionsColumn
            spacing: 8
            Keys.onPressed: event => {
                if (!(event.modifiers & Qt.ControlModifier)) return
                if (event.key === Qt.Key_F) {
                    event.accepted = true
                    readerHeader.openSearch()
                } else if (event.key === Qt.Key_N && (mail.configured || mail.demo)) {
                    event.accepted = true
                    listOptions.close(); window.compose("new")
                } else if (event.key === Qt.Key_R && !mail.busy) {
                    event.accepted = true
                    listOptions.close(); mail.refresh()
                }
            }
            LiquidCheckBox {
                id: unreadFilterCheck
                objectName: "unreadFilterCheck"
                Layout.fillWidth: true
                text: qsTr("Unread messages only (cached)")
                checked: mail.unreadOnly
                onToggled: mail.unreadOnly = checked
            }
            LiquidCheckBox {
                objectName: "groupThreadsCheck"
                Layout.fillWidth: true
                text: qsTr("按会话聚合")
                checked: window.groupThreads
                onToggled: window.groupThreads = checked
            }
            LiquidCheckBox {
                objectName: "notifyCheck"
                Layout.fillWidth: true
                text: qsTr("新邮件通知")
                checked: mail.notifyEnabled
                onToggled: mail.notifyEnabled = checked
            }
            LiquidCheckBox {
                objectName: "summaryCheck"
                Layout.fillWidth: true
                text: qsTr("Show message summaries")
                checked: window.showSummary
                onToggled: window.showSummary = checked
            }
            LiquidCheckBox {
                objectName: "richTextCheck"
                Layout.fillWidth: true
                text: qsTr("富文本显示")
                checked: window.showRichText
                onToggled: window.showRichText = checked
            }
            LiquidPopoverItem {
                objectName: "overflowRefresh"
                Layout.fillWidth: true
                visible: readerHeader.compactActions
                text: qsTr("Check for mail"); iconName: "refreshRounded"
                enabled: !mail.busy && (mail.configured || mail.demo)
                onTriggered: { listOptions.close(); mail.refresh() }
            }
            LiquidPopoverItem {
                objectName: "overflowCompose"
                Layout.fillWidth: true
                visible: readerHeader.compactActions
                text: qsTr("New message"); iconName: "editRounded"
                enabled: mail.configured || mail.demo
                onTriggered: { listOptions.close(); window.compose("new") }
            }
        }
    }
    LiquidPopover {
        id: moreMenu
        objectName: "moreMessageMenu"
        sourceItem: chromePlanes
        anchorItem: messageOptionsGroup
        LiquidPopoverItem { text: qsTr("Reply"); iconName: "undo"; enabled: !mail.sending; onTriggered: window.compose("reply") }
        LiquidPopoverItem { objectName: "moreReplyAll"; text: qsTr("Reply to all"); iconName: "arrows"; enabled: window.hasMessage && !mail.sending; onTriggered: window.compose("replyAll") }
        LiquidPopoverItem { text: qsTr("Forward"); iconName: "forward"; enabled: !mail.sending; onTriggered: window.compose("forward") }
        LiquidPopoverItem { text: qsTr("Message details"); iconName: "reminder"; onTriggered: window.detailsVisible = true }
        LiquidPopoverItem {
            text: qsTr("删除"); iconName: "trash"
            enabled: window.hasMessage && window.width < 1100
            onTriggered: mail.deleteMessage(window.selectedIndex())
        }
        LiquidPopoverItem {
            text: qsTr("归档"); iconName: "archive"
            enabled: window.hasMessage && window.width < 1100
            onTriggered: mail.archiveMessage(window.selectedIndex())
        }
        LiquidPopoverItem {
            text: window.message.flagged ? qsTr("取消旗标") : qsTr("标为旗标"); iconName: "flag"
            enabled: window.hasMessage && window.width < 1100
            onTriggered: mail.toggleFlagged(window.selectedIndex())
        }
    }
    MailComposer { id: composer; mail: window.mail; sourceItem: chromePlanes; hostWindow: window }
    AccountDialog { id: accountDialog; mail: window.mail; sourceItem: chromePlanes; hostWindow: window }
    LiquidDialog {
        id: deleteAccountDialog
        objectName: "deleteAccountDialog"
        parent: Overlay.overlay
        property string targetId: ""
        property string targetName: ""
        width: Math.min(420, window.width - 32)
        anchors.centerIn: parent
        title: qsTr("删除账号？")
        contentItem: Text {
            text: qsTr("%1 的本地缓存和记住的口令会被删除，服务器上的邮件不受影响。").arg(deleteAccountDialog.targetName)
            color: AppTheme.text
            width: deleteAccountDialog.availableWidth
            wrapMode: Text.Wrap
        }
        footer: Item {
            implicitHeight: MailStyle.footerHeight
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20; anchors.rightMargin: 20; anchors.topMargin: 12; anchors.bottomMargin: 20
                spacing: 8
                Item { Layout.fillWidth: true }
                LiquidDialogButton { text: qsTr("取消"); onClicked: deleteAccountDialog.close() }
                LiquidDialogButton {
                    objectName: "deleteAccountButton"
                    text: qsTr("删除")
                    destructive: true
                    onClicked: {
                        const id = deleteAccountDialog.targetId
                        deleteAccountDialog.close()
                        window.mail.removeAccount(id)
                    }
                }
            }
        }
    }
    FileDialog {
        id: saveAttachmentDialog
        property int attachmentIndex: -1
        title: qsTr("Save attachment")
        fileMode: FileDialog.SaveFile
        onAccepted: mail.saveAttachment(attachmentIndex, selectedFile.toString())
    }
    Shortcut { sequence: "Ctrl+N"; enabled: !composer.visible && !accountDialog.visible && (mail.configured || mail.demo); onActivated: window.compose("new") }
    Shortcut { sequence: "Ctrl+F"; enabled: !composer.visible && !accountDialog.visible; onActivated: readerHeader.openSearch() }
    Shortcut {
        sequence: "Escape"
        enabled: !composer.visible && !accountDialog.visible && !window.detailsVisible && !moreMenu.visible
        onActivated: {
            if (window.selectionSize > 0) { window.clearSelection(); return }
            if (window.detailsVisible) { window.detailsVisible = false; return }
            if (window.width < 1080 && window.sidebarExpanded) window.sidebarExpanded = false
            else if (window.narrow && window.readerVisible) window.readerVisible = false
            else if (toolbarSearch.active) readerHeader.closeSearch()
        }
    }
    Shortcut { sequence: "Ctrl+R"; enabled: !composer.visible && !accountDialog.visible && !mail.busy; onActivated: mail.refresh() }
    Shortcut {
        sequence: "Delete"
        enabled: !composer.visible && !accountDialog.visible && window.hasMessage
        onActivated: mail.deleteMessage(window.selectedIndex())
    }
    Shortcut {
        sequence: "Ctrl+Backspace"
        enabled: !composer.visible && !accountDialog.visible && window.hasMessage
        onActivated: mail.deleteMessage(window.selectedIndex())
    }
    Shortcut {
        sequence: "Ctrl+Shift+U"
        enabled: !composer.visible && !accountDialog.visible && window.hasMessage
        onActivated: mail.setMessageRead(window.selectedIndex(), !!window.message.unread)
    }
    Shortcut {
        sequence: "Ctrl+Shift+L"
        enabled: !composer.visible && !accountDialog.visible && window.hasMessage
        onActivated: mail.toggleFlagged(window.selectedIndex())
    }
    Shortcut {
        sequence: "Ctrl+Shift+A"
        enabled: !composer.visible && !accountDialog.visible && window.hasMessage
        onActivated: mail.archiveMessage(window.selectedIndex())
    }
}

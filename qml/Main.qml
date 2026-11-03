import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import Kos.Ui

ApplicationWindow {
    id: window
    required property var mail
    property var desktop: null
    property var i18n: null
    property string lifecycleNotice: ""
    objectName: "mailWindow"
    width: 1180
    height: 760
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
    property bool showAvatars: false
    property bool groupThreads: true
    property bool showRichText: true
    MailSelection { id: mailSelection }
    readonly property int selectionSize: mailSelection.selectionSize
    function clearSelection() { mailSelection.clearSelection() }
    function selectedIndexList() { return mailSelection.selectedIndexList() }
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
    property int sidebarPreferredWidth: 189
    property int listPreferredWidth: 280
    readonly property int sidebarWidth: Math.max(152, Math.min(340, sidebarPreferredWidth,
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
    function value(map, key) { return MailLabels.value(map, key) }
    function selectedIndex() {
        if (!hasMessage) return -1
        return mail.messages.indexOf(window.value(message, "folder") || mail.currentFolder, window.value(message, "uidValidity"), mail.selectedUid)
    }
    function mailboxTitle(folder) { return MailLabels.mailboxTitle(folder) }
    function folderIcon(id) { return MailLabels.folderIcon(id) }
    function draftStateLabel(state) { return MailLabels.draftStateLabel(state) }
    function folderTitle() {
        if (draftsVisible) return qsTr("Drafts")
        if (mail.smartMailbox) return MailLabels.smartMailboxTitle(mail.smartMailbox)
        const folders = mail.folders || []
        for (let i = 0; i < folders.length; ++i)
            if (folders[i].id === mail.currentFolder) return mailboxTitle(folders[i])
        return mail.currentFolder || qsTr("Inbox")
    }
    function dateLabel(date) { return MailLabels.dateLabel(date) }
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
        if (scenario === "account-add") { accountDialog.addAccount(); return }
        if (scenario === "flagged") { mail.selectSmartMailbox("flagged"); return }
        if (scenario === "avatars") { window.showAvatars = true; return }
        if (scenario === "more-menu") { moreMenu.openAtAnchor(); return }
        if (scenario === "message-menu") { messagePane.showReviewMenu(); return }
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
    function prepareForQuit() {
        if (mail.sending || mail.sendPending) {
            lifecycleNotice = qsTr("邮件正在发送或等待发送，请等待完成或撤销发送后再退出。")
            return false
        }
        if (composer.visible && !composer.persist()) {
            composer.forceActiveFocus()
            return false
        }
        lifecycleNotice = ""
        return true
    }
    onClosing: close => {
        if (composer.visible && !composer.persist()) {
            close.accepted = false
            composer.forceActiveFocus()
            return
        }
        if (desktop && desktop.hideToTray()) {
            close.accepted = false
            return
        }
        close.accepted = window.prepareForQuit()
    }
    Connections {
        target: mail
        function onStatusChanged() { if (!mail.sending && !mail.sendPending) window.lifecycleNotice = "" }
        function onSendPendingChanged() { if (!mail.sending && !mail.sendPending) window.lifecycleNotice = "" }
        function onSelectionChanged() { if (window.narrow && window.hasMessage) window.readerVisible = true }
        function onFolderChanged() { window.clearSelection() }
        function onAccountChanged() {
            // switchAccount() already empties the message model; the per-row menus
            // live inside those delegates and are torn down with them.
            mailSelection.clearSelection()
            mailSelection.selectionAnchor = -1
            mailSelection.collapsedThreads = ({})
            window.detailsVisible = false
            listOptions.close()
        }
    }
    Connections {
        target: mail.messages
        function onModelReset() { window.clearSelection() }
        function onRowsRemoved() { window.clearSelection() }
        function onRowsInserted() { window.clearSelection() }
        function onRowsMoved() { window.clearSelection() }
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
                    objectName: "mailTitle"
                    x: 14
                    y: (parent.height - height) / 2
                    width: parent.width - 28; height: 26
                    text: qsTr("Mail"); font.pixelSize: 24; font.weight: Font.Bold
                    color: window.ink; elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
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
                    y: windowChrome.controlsCenterY - height / 2
                    objectName: "unreadFilterButton"
                    visible: messagePane.visible && !window.draftsVisible
                    iconOnly: true
                    iconName: "filter"
                    iconSize: 14
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

            MailToolbar {
                id: readerHeader
                mail: window.mail
                selection: mailSelection
                backdrop: chromeBackdrop
                surface: chromePlanes
                windowWidth: window.width
                controlsLeft: windowChrome.controlsLeft - x
                controlsCenterY: windowChrome.controlsCenterY
                narrow: window.narrow
                sidebarVisible: window.sidebarVisible
                readerVisible: window.readerVisible
                draftsVisible: window.draftsVisible
                liquidControls: window.liquidControls
                hasMessage: window.hasMessage
                visible: reader.visible || readerHeader.searching
                x: contentScene.x + (window.narrow ? 0 : reader.x)
                width: Math.max(0, windowChrome.controlsLeft - x)
                height: parent.height
                onMessageCommandRequested: function(command) {
                    const row = window.selectedIndex()
                    if (row < 0) return
                    if (command === 0) mail.deleteMessage(row)
                    else if (command === 1) mail.archiveMessage(row)
                    else mail.toggleFlagged(row)
                }
                onComposeRequested: function(mode) { window.compose(mode) }
                onOptionsRequested: listOptions.open()
                onMoreRequested: moreMenu.openAtAnchor()
                onDetailsRequested: window.detailsVisible = true
                onBackRequested: window.readerVisible = false
                onLayoutChanged: listOptions.close()
                onSearchRequested: {
                    listOptions.close(); moreMenu.close(); window.detailsVisible = false
                    window.draftsVisible = false; window.readerVisible = false
                    if (window.narrow) window.sidebarExpanded = false
                }
            }

            Item {
                id: sidebarToggleHost
                x: window.sidebarVisible ? 0 : 14
                width: window.sidebarVisible ? sidebarPlane.width : sidebarToggleButton.width
                height: parent.height
                KosSidebarToggle {
                    id: sidebarToggleButton
                    objectName: "sidebarToggleButton"
                    anchors.right: parent.right
                    anchors.rightMargin: window.sidebarVisible ? 12 : 0
                    y: windowChrome.controlsCenterY - height / 2
                    implicitWidth: MailStyle.toolbarButtonHeight
                    implicitHeight: implicitWidth
                    // Same shared sidebar control as Weather; never use panelLeft.
                    glyphColor: AppTheme.text
                    glyphPaneColor: glyphColor
                    Accessible.name: window.sidebarVisible ? qsTr("Hide sidebar") : qsTr("Show sidebar")
                    background: LiquidActionButton {
                        enabled: false
                        Accessible.ignored: true
                        materialForm: !window.liquidControls
                        backdrop: chromeBackdrop
                        liquidIntensity: "strong"
                    }
                    focusPolicy: Qt.TabFocus
                    onClicked: window.toggleSidebar()
                }
            }
        }

        MailSidebar {
            id: sidebar
            mail: window.mail
            surface: chromePlanes
            sidebarAccounts: window.sidebarAccounts
            draftsVisible: window.draftsVisible
            unreadRevision: window.unreadRevision
            visible: window.sidebarVisible
            anchors.left: parent.left
            anchors.top: floatingHeader.bottom
            anchors.bottom: parent.bottom
            width: sidebarPlane.width
            z: 1
            onEditAccountRequested: function(account) { accountDialog.editAccount(account) }
            onRemoveAccountRequested: function(accountId, name) { deleteAccountDialog.targetId = accountId; deleteAccountDialog.targetName = name; deleteAccountDialog.open() }
            onAddAccountRequested: accountDialog.addAccount()
            onMailboxOpened: { window.draftsVisible = false; window.readerVisible = false; if (window.width < 1080) window.sidebarExpanded = false }
            onDraftsRequested: { window.draftsVisible = true; window.readerVisible = false; if (window.width < 1080) window.sidebarExpanded = false }
        }

        RowLayout {
            id: contentScene
            objectName: "contentScene"
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: floatingHeader.bottom
            anchors.bottom: parent.bottom
            spacing: 0
            z: 1

            MessagePane {
                localeName: window.i18n ? window.i18n.localeName : Qt.locale().name
                id: messagePane
                surface: chromePlanes
                mail: window.mail
                selection: mailSelection
                draftsVisible: window.draftsVisible
                groupThreads: window.groupThreads
                showSummary: window.showSummary
                showAvatars: window.showAvatars
                visible: !window.narrow || !window.readerVisible
                Layout.preferredWidth: window.listWidth
                Layout.minimumWidth: window.narrow ? 260 : window.listWidth
                Layout.maximumWidth: window.narrow ? -1 : window.listWidth
                Layout.fillWidth: window.narrow
                Layout.fillHeight: true
                onMessageOpened: window.readerVisible = true
                onComposeRequested: function(mode) { window.compose(mode) }
                onAddAccountRequested: accountDialog.editAccount()
                onEditDraftRequested: function(draftId) { composer.editDraft(mail.loadDraft(draftId)) }
            }

            MessageReader {
                localeName: window.i18n ? window.i18n.localeName : Qt.locale().name
                id: reader
                mail: window.mail
                message: window.message
                hasMessage: window.hasMessage
                draftsVisible: window.draftsVisible
                detailsVisible: window.detailsVisible
                showRichText: window.showRichText
                compact: window.width < 1100
                visible: !window.narrow || window.readerVisible
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0
                onDetailsRequested: window.detailsVisible = !window.detailsVisible
                onCloseDetailsRequested: window.detailsVisible = false
                onSaveAttachmentRequested: function(index) { saveAttachmentDialog.attachmentIndex = index; saveAttachmentDialog.open() }
                onAddAccountRequested: accountDialog.editAccount()
            }

        }
        Rectangle {
            objectName: "messageColumnRule"
            x: contentScene.x + messagePane.x + messagePane.width - width
            y: 0
            width: 1
            height: parent.height - y
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
            height: parent.height - y
            z: 4
            cursorShape: Qt.SplitHCursor
            hoverEnabled: true
            activeFocusOnTab: true
            preventStealing: true
            Accessible.role: Accessible.Slider
            Accessible.name: sidebarHandle ? qsTr("Mailbox column width") : qsTr("Message column width")
            Accessible.description: qsTr("Drag or use Left and Right to resize. Double-click to reset.")
            function resizeTo(value) {
                if (sidebarHandle) window.sidebarPreferredWidth = Math.round(Math.max(152, Math.min(340, value, window.width - 560)))
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
                if (sidebarHandle) window.sidebarPreferredWidth = 189
                else window.listPreferredWidth = 280
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
            id: transientNotice
            objectName: "transientNotice"
            readonly property string message: window.lifecycleNotice || mail.error
                || ((!mail.demo && (mail.busy || mail.sending || mail.sendPending)) ? mail.status : "")
            visible: message.length > 0
            anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.margins: 16
            width: Math.min(460, parent.width - 32)
            height: noticeRow.implicitHeight + 24
            z: 10
            radius: 10
            color: AppTheme.contentBackground
            border.width: 1
            border.color: mail.error ? AppTheme.destructive : window.rule
            RowLayout {
                id: noticeRow
                anchors.fill: parent; anchors.margins: 12
                spacing: 12
                Text {
                    text: transientNotice.message
                    color: mail.error ? AppTheme.destructive : window.ink
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                MailAction {
                    visible: !!mail.error || !!window.lifecycleNotice
                    implicitHeight: 24
                    text: qsTr("Dismiss")
                    onClicked: { mail.clearError(); window.lifecycleNotice = "" }
                }
            }
        }

    }

    KosWindowResizeHandles { targetWindow: window }

    LiquidContentPopover {
        id: listOptions
        objectName: "listOptionsPopover"
        sourceItem: chromePlanes
        anchorItem: readerHeader.optionsAnchor
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
                objectName: "senderInitialsCheck"
                Layout.fillWidth: true
                text: qsTr("Sender initials")
                checked: window.showAvatars
                onToggled: window.showAvatars = checked
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
        LiquidPopoverItem {
            objectName: "moreDisplayOptions"
            visible: readerHeader.compactOptions
            // Menu's ListView retains invisible delegates; collapse the row too.
            implicitHeight: readerHeader.compactOptions ? 30 : 0
            height: implicitHeight
            text: qsTr("显示"); iconName: "sliders"
            onTriggered: { moreMenu.close(); listOptions.openAtAnchor() }
        }
        LiquidPopoverItem {
            objectName: "languageSystem"
            text: qsTr("Language: follow system") + (window.i18n && window.i18n.language === "system" ? " ✓" : "")
            onTriggered: { if (window.i18n) window.i18n.language = "system"; moreMenu.close() }
        }
        LiquidPopoverItem {
            objectName: "languageChinese"
            text: "简体中文" + (window.i18n && window.i18n.language === "zh_CN" ? " ✓" : "")
            onTriggered: { if (window.i18n) window.i18n.language = "zh_CN"; moreMenu.close() }
        }
        LiquidPopoverItem {
            objectName: "languageEnglish"
            text: "English" + (window.i18n && window.i18n.language === "en" ? " ✓" : "")
            onTriggered: { if (window.i18n) window.i18n.language = "en"; moreMenu.close() }
        }
        LiquidPopoverItem {
            text: qsTr("Switch light / dark appearance"); iconName: "theme"
            onTriggered: { moreMenu.close(); AppTheme.appearanceMode = AppTheme.dark ? "light" : "dark" }
        }
        sourceItem: chromePlanes
        anchorItem: readerHeader.optionsAnchor
        LiquidPopoverItem { text: qsTr("Reply"); iconName: "undo"; enabled: !mail.sending; onTriggered: window.compose("reply") }
        LiquidPopoverItem { objectName: "moreReplyAll"; text: qsTr("Reply to all"); iconName: "arrows"; enabled: window.hasMessage && !mail.sending; onTriggered: window.compose("replyAll") }
        LiquidPopoverItem { text: qsTr("Forward"); iconName: "forward"; enabled: !mail.sending; onTriggered: window.compose("forward") }
        LiquidPopoverItem { text: qsTr("Message details"); iconName: "reminder"; onTriggered: window.detailsVisible = true }
        LiquidPopoverItem {
            text: qsTr("删除"); iconName: "trash"
            enabled: window.hasMessage && !readerHeader.messageOpsVisible
            onTriggered: mail.deleteMessage(window.selectedIndex())
        }
        LiquidPopoverItem {
            text: qsTr("归档"); iconName: "archive"
            enabled: window.hasMessage && !readerHeader.messageOpsVisible
            onTriggered: mail.archiveMessage(window.selectedIndex())
        }
        LiquidPopoverItem {
            text: window.message.flagged ? qsTr("取消旗标") : qsTr("标为旗标"); iconName: "flag"
            enabled: window.hasMessage && !readerHeader.messageOpsVisible
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
    Shortcut { sequence: "Ctrl+Q"; onActivated: { if (window.desktop) window.desktop.requestQuit(); else if (window.prepareForQuit()) window.close() } }
    Shortcut { sequence: "Ctrl+N"; enabled: !composer.visible && !accountDialog.visible && (mail.configured || mail.demo); onActivated: window.compose("new") }
    Shortcut { sequence: "Ctrl+F"; enabled: !composer.visible && !accountDialog.visible; onActivated: readerHeader.openSearch() }
    Shortcut {
        sequence: "Escape"
        enabled: !composer.visible && !accountDialog.visible && !window.detailsVisible && !moreMenu.visible
        onActivated: {
            if (window.selectionSize > 0) { window.clearSelection(); return }
            if (window.detailsVisible) { window.detailsVisible = false; return }
            if (readerHeader.searchActive) { readerHeader.closeSearch(); return }
            if (window.width < 1080 && window.sidebarExpanded) window.sidebarExpanded = false
            else if (window.narrow && window.readerVisible) window.readerVisible = false
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

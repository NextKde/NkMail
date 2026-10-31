import QtQuick
import QtTest
import Kos.Ui
import NextKde.Mail.TestViews 1.0 as App

TestCase {
    id: testCase
    name: "MailUi"
    when: windowShown
    width: 1280
    height: 820
    property var app: null

    Item {
        id: mock
        property alias messages: messageModel
        property var folders: [{id: "INBOX", title: "Inbox", total: 1}]
        property var drafts: []
        property var account: ({id: "example", name: "Alex Example", address: "alex@example.invalid"})
        property var selectedMessage: ({uid: "1", uidValidity: "1", sender: "Studio Example", senderAddress: "studio@example.invalid", subject: "A quieter inbox", date: "2026-10-29T09:00:00Z", to: "alex@example.invalid", cc: "", messageId: "<one@example.invalid>", text: "<img src=https://example.invalid/tracker>\nPlain text only.", attachments: []})
        property string selectedUid: "1"
        property string currentFolder: "INBOX"
        property string status: "Demo"
        property string error: ""
        property bool busy: false
        property bool sending: false
        property bool configured: true
        property bool demo: true
        property bool hasMore: false
        property string search: ""
        property bool unreadOnly: false
        property bool failSave: false
        property int refreshCount: 0
        property string lastComposeMode: ""
        property int saveCount: 0
        property var lastSaved: ({})
        signal selectionChanged()
        signal draftAccepted(string id)
        ListModel {
            id: messageModel
            ListElement { uid: "1"; uidValidity: "1"; folder: "INBOX"; sender: "Studio Example"; senderAddress: "studio@example.invalid"; subject: "A quieter inbox"; date: "2026-10-29T09:00:00Z"; preview: "A synthetic message."; unread: true; flagged: false; hasAttachments: false }
        }
        function configureAccount(config, password, remember) {}
        function refresh() { refreshCount++ }
        function selectFolder(folder) { currentFolder = folder }
        function loadMore() {}
        function openMessage(uid, validity) { selectedUid = uid }
        function newDraft(mode) { lastComposeMode = mode; return {id: "test-draft", to: "", cc: "", bcc: "", subject: "", body: "", inReplyTo: "", references: "", state: "draft", attachments: []} }
        function loadDraft(id) { return newDraft("new") }
        function saveDraft(draft) {
            saveCount++
            if (failSave) return ({})
            lastSaved = JSON.parse(JSON.stringify(draft))
            return lastSaved
        }
        function discardDraft(id) {}
        function addAttachment(url) { return ({}) }
        function saveAttachment(index, url) {}
        function sendDraft(draft) { sending = true }
        function clearError() { error = "" }
        function disconnectAccount() { configured = false }
    }
    Component { id: appComponent; App.Main {} }
    function init() {
        AppTheme.reduceMotion = true
        AppTheme.reduceTransparency = false
        AppTheme.materialMode = "auto"
        AppTheme.appearanceMode = "light"
        failOnWarning(/.*/)
        mock.failSave = false; mock.saveCount = 0; mock.lastSaved = ({})
        mock.refreshCount = 0; mock.search = ""; mock.unreadOnly = false; mock.lastComposeMode = ""
        mock.busy = false; mock.configured = true; mock.selectedUid = "1"
        mock.folders = [{id: "INBOX", title: "Inbox", total: 1}]
        mock.currentFolder = "INBOX"; mock.drafts = []
        mock.sending = false; mock.demo = true; mock.error = ""
        app = createTemporaryObject(appComponent, testCase, {mail: mock})
        verify(app !== null)
        wait(80)
    }
    function cleanup() {
        if (app) app.destroy()
        app = null
        wait(30)
    }
    function activateApp() {
        app.requestActivate()
        wait(40)
    }
    function clickInScene(item) {
        const scene = findChild(app, "mailScene")
        const point = item.mapToItem(scene, item.width / 2, item.height / 2)
        mouseClick(scene, point.x, point.y)
    }
    function groupButton(group, index) {
        const buttons = []
        function collect(item) {
            if (item.iconOnly && item.iconName === group.icons[index]) buttons.push(item)
            for (const child of item.children) collect(child)
        }
        collect(group)
        compare(buttons.length, 1)
        return buttons[0]
    }
    function isWithin(item, ancestor) {
        while (item) {
            if (item === ancestor) return true
            item = item.parent
        }
        return false
    }
    function moreCommand(iconName) {
        const commands = []
        function collect(item) {
            if (item.iconName === iconName && item.text !== undefined) commands.push(item)
            for (const child of item.children) collect(child)
        }
        collect(findChild(app, "moreMessageMenu").contentItem)
        compare(commands.length, 1)
        return commands[0]
    }
    function verifySeparate(first, second) {
        if (!first.visible || !second.visible) return
        const scene = findChild(app, "mailScene")
        const a = first.mapToItem(scene, 0, 0)
        const b = second.mapToItem(scene, 0, 0)
        verify(a.x + first.width <= b.x || b.x + second.width <= a.x
            || a.y + first.height <= b.y || b.y + second.height <= a.y,
            first.objectName + " overlaps " + second.objectName)
    }
    function test_default_chinese_interface_preserves_mail_content() {
        compare(Qt.locale().name, "zh_CN")
        compare(findChild(app, "mailboxGroupTitle").text, "邮件夹")
        const inbox = findChild(app, "mailboxFolders").itemAt(0)
        compare(inbox.text, "收件箱")
        compare(inbox.Accessible.name, "收件箱")
        compare(mock.folders[0].id, "INBOX")
        compare(mock.folders[0].title, "Inbox")
        compare(findChild(app, "folderTitle").text, "收件箱")
        compare(findChild(app, "draftFolderButton").text, "草稿")
        compare(findChild(app, "mailboxActionGroup").labels[0], "收取邮件")
        compare(findChild(app, "mailboxActionGroup").labels[1], "新建邮件")
        compare(app.mailboxTitle({id: "Sent", title: "Sent"}), "已发送")
        compare(app.mailboxTitle({id: "projects", title: "Project Phoenix"}), "Project Phoenix")
        compare(app.dateLabel("Yesterday"), "昨天")
        compare(app.draftStateLabel("draft"), "草稿")
        compare(app.draftStateLabel("unknown"), "未知")
        compare(mock.selectedMessage.subject, "A quieter inbox")
        compare(qsTranslate("nkmail::MailController", "Add a recipient"), "添加收件人")
        app.showReview("compose")
        compare(findChild(app, "composerDialog").title, "新建邮件")
        app.showReview("account")
        compare(findChild(app, "accountDialog").title, "邮件账户")
    }
    function test_header_has_no_horizontal_rule_and_column_rule_spans_scene() {
        const line = findChild(app, "messageColumnRule")
        const header = findChild(app, "floatingHeader")
        const scene = findChild(app, "mailScene")
        const content = findChild(app, "contentScene")
        const list = findChild(app, "messagePane")
        const divider = findChild(app, "messageDivider")
        compare(findChild(app, "contentHeaderRule"), null)
        compare(findChild(app, "sidebarHeaderRule"), null)
        verify(line !== null)
        compare(line.parent, scene)
        compare(line.width, 1)
        compare(line.z, 2)
        for (const appearance of ["light", "dark"]) {
            AppTheme.appearanceMode = appearance
            compare(line.color, AppTheme.border)
        }
        // The former body-only right rule must not remain underneath the new one.
        for (const child of list.children)
            verify(!(child.width === 1 && child.height === list.height && child.color !== undefined),
                "messagePane must not retain its old right rule")
        for (const height of [520, 820]) {
            app.height = height
            app.sidebarPreferredWidth = 290
            for (const width of [580, 650, 799, 800, 820, 1079, 1080, 1280]) {
                app.width = width
                for (const expanded of [true, false]) {
                    app.sidebarCollapsed = !expanded
                    app.sidebarExpanded = expanded
                    for (const reading of [false, true]) {
                        app.readerVisible = reading
                        wait(25)
                        compare(header.height, 35)
                        compare(content.y, 35)
                        compare(line.visible, !app.narrow)
                        compare(line.x, content.x + list.x + list.width - 1)
                        compare(line.y, 0)
                        compare(line.height, scene.height)
                        compare(line.mapToItem(scene, 0, 0).y, 0)
                        compare(line.y + line.height, scene.height)
                        // Only the visual rule reaches y=0; the resize hit target stays below the header.
                        compare(divider.y, 35)
                        compare(divider.y + divider.height, scene.height)
                    }
                }
            }
        }
        app.width = 1280
        app.sidebarCollapsed = false
        for (const preferred of [260, 410, 520]) {
            app.listPreferredWidth = preferred
            wait(25)
            compare(line.x, content.x + list.x + list.width - 1)
            compare(line.height, scene.height)
        }
    }
    function test_mailbox_rows_follow_shared_gallery_specification() {
        const navigation = findChild(app, "mailboxNavigation")
        const group = findChild(app, "mailboxGroupTitle")
        const inbox = findChild(app, "mailboxFolders").itemAt(0)
        const drafts = findChild(app, "draftFolderButton")
        compare(navigation.spacing, 0)
        compare(group.font.pixelSize, 11)
        compare(group.font.weight, Font.DemiBold)
        compare(group.topPadding, 12)
        compare(group.bottomPadding, 4)
        compare(group.font.letterSpacing, 0)
        for (const row of [inbox, drafts]) {
            verify(row !== null)
            compare(row.specificationForm, true)
            compare(row.height, 30)
            compare(row.implicitHeight, 30)
            compare(row.specificationFontSize, 12)
            compare(row.font.pixelSize, 12)
            compare(row.font.weight, Font.Medium)
            compare(row.leftPadding, 12)
            compare(row.specificationSelectedVerticalInset, 2)
            compare(row.specificationSelectedFillColor, AppTheme.accent)
            compare(row.specificationSelectedTextColor, Qt.rgba(1, 1, 1, 1))
            compare(row.specificationHoverFillColor, Qt.rgba(219 / 255, 219 / 255, 221 / 255, 1))
            compare(findChild(row, "mailboxCount").font.pixelSize, 12)
        }
        compare(drafts.y, inbox.y + inbox.height)
        compare(inbox.checked, true)
        compare(inbox.iconColor, inbox.specificationSelectedTextColor)
        compare(findChild(inbox, "mailboxCount").color, inbox.specificationSelectedTextColor)
        compare(findChild(inbox, "mailboxCount").text, "1")
        compare(findChild(drafts, "mailboxCount").visible, false)
        AppTheme.appearanceMode = "dark"
        compare(inbox.specificationHoverFillColor, Qt.rgba(1, 1, 1, .08))
        compare(inbox.iconColor, inbox.specificationSelectedTextColor)
    }
    function test_mailbox_count_reserves_space_for_long_titles() {
        mock.folders = [{id: "INBOX", title: "A very long synthetic mailbox title that must be elided", total: 123456}]
        const inbox = findChild(app, "mailboxFolders").itemAt(0)
        verify(inbox !== null)
        const count = findChild(inbox, "mailboxCount")
        tryCompare(count, "text", "123456")
        verify(count.visible)
        compare(inbox.rightPadding, 12 + count.implicitWidth + 8)
        verify(inbox.contentItem.width > 0)
        verify(inbox.contentItem.x + inbox.contentItem.width <= count.x - 8)
        compare(count.x + count.width, inbox.width - 12)
        mock.folders = [{id: "INBOX", title: "Inbox", total: -1}]
        const withoutCount = findChild(app, "mailboxFolders").itemAt(0)
        tryCompare(findChild(withoutCount, "mailboxCount"), "visible", false)
        compare(withoutCount.rightPadding, 12)
        mock.drafts = [{id: "synthetic-draft"}]
        const draftCount = findChild(findChild(app, "draftFolderButton"), "mailboxCount")
        tryCompare(draftCount, "text", "1")
        compare(draftCount.visible, true)
    }
    function test_mailbox_selection_uses_business_state_and_compact_close() {
        activateApp()
        const inbox = findChild(app, "mailboxFolders").itemAt(0)
        const drafts = findChild(app, "draftFolderButton")
        clickInScene(inbox)
        compare(inbox.checked, true)
        clickInScene(drafts)
        compare(app.draftsVisible, true)
        compare(drafts.checked, true)
        compare(inbox.checked, false)
        drafts.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(drafts.checked, true)
        compare(app.draftsVisible, true)
        inbox.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(mock.currentFolder, "INBOX")
        compare(app.draftsVisible, false)
        compare(inbox.checked, true)
        compare(drafts.checked, false)
        app.width = 650
        app.sidebarExpanded = true
        wait(25)
        clickInScene(drafts)
        compare(app.sidebarVisible, false)
        compare(app.draftsVisible, true)
        app.sidebarExpanded = true
        wait(25)
        clickInScene(inbox)
        compare(app.sidebarVisible, false)
        compare(app.draftsVisible, false)
        compare(inbox.checked, true)
    }
    function test_sidebar_glyph_matches_weather_and_calendar() {
        const toggle = findChild(app, "sidebarToggleButton")
        const frame = findChild(app, "sidebarGlyphFrame")
        const pane = findChild(app, "sidebarGlyphPane")
        compare(findChild(app, "sidebarComposeButton"), null)
        compare(findChild(app, "sidebarHeader"), null)
        verify(findChild(app, "mailboxActionGroup").visible)
        compare(frame.width, 16)
        compare(frame.height, 13)
        compare(frame.radius, 3.5)
        compare(frame.border.width, 1.25)
        compare(pane.x, 3.5)
        compare(pane.y, 2.5)
        compare(pane.width, 2.5)
        compare(pane.height, 8)
        compare(pane.radius, 1.25)
        compare(frame.border.color, toggle.iconColor)
        compare(pane.color, toggle.iconColor)
        AppTheme.appearanceMode = "dark"
        compare(frame.border.color, toggle.iconColor)
        compare(pane.color, toggle.iconColor)
    }
    function test_sidebar_toggle_closes_and_restores_wide_layout() {
        activateApp()
        const toggle = findChild(app, "sidebarToggleButton")
        const plane = findChild(app, "sidebarPlane")
        const body = findChild(app, "sidebarBody")
        const content = findChild(app, "contentScene")
        const search = findChild(app, "searchField")
        compare(toggle.width, 26)
        compare(toggle.height, 26)
        compare(toggle.accessibleText, "隐藏侧边栏")
        compare(toggle.liquidIntensity, "strong")
        compare(toggle.backdrop, findChild(app, "chromeBackdrop"))
        compare(toggle.materialForm, true) // Software rendering uses the solid fallback.
        compare(app.sidebarVisible, true)
        clickInScene(toggle)
        compare(app.sidebarVisible, false)
        compare(app.sidebarCollapsed, true)
        compare(body.visible, false)
        compare(plane.width, 0)
        compare(body.width, 0)
        compare(content.x, 0)
        compare(toggle.accessibleText, "显示侧边栏")
        compare(findChild(app, "toolbarSearch").active, false)
        compare(search.visible, false)
        compare(findChild(app, "mailboxActionGroup").visible, true)
        clickInScene(toggle)
        compare(app.sidebarVisible, true)
        compare(plane.width, app.sidebarWidth)
        compare(content.x, app.sidebarWidth)
        compare(app.sidebarPreferredWidth, 236)
        compare(toggle.accessibleText, "隐藏侧边栏")
    }
    function test_sidebar_toggle_in_compact_list_and_reader() {
        activateApp()
        app.width = 650
        const toggle = findChild(app, "sidebarToggleButton")
        for (const reading of [false, true]) {
            app.readerVisible = reading
            wait(25)
            compare(app.sidebarVisible, false)
            verify(toggle.visible)
            clickInScene(toggle)
            compare(app.sidebarVisible, true)
            compare(app.sidebarExpanded, true)
            compare(app.readerVisible, reading)
            clickInScene(toggle)
            compare(app.sidebarVisible, false)
            compare(app.readerVisible, reading)
            clickInScene(toggle)
            keyClick(Qt.Key_Escape)
            compare(app.sidebarVisible, false)
            compare(app.readerVisible, reading)
        }
    }
    function test_sidebar_toggle_keyboard_and_breakpoint_memory() {
        activateApp()
        const toggle = findChild(app, "sidebarToggleButton")
        toggle.forceActiveFocus()
        tryCompare(toggle, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(app.sidebarVisible, false)
        compare(toggle.accessibleText, "显示侧边栏")
        keyClick(Qt.Key_Space)
        compare(app.sidebarVisible, true)
        app.width = 1079
        wait(25)
        compare(app.width, 1079)
        compare(app.sidebarVisible, false)
        clickInScene(toggle)
        compare(app.sidebarExpanded, true)
        app.width = 1080
        wait(25)
        compare(app.width, 1080)
        clickInScene(toggle)
        compare(app.sidebarCollapsed, true)
        compare(app.sidebarVisible, false)
        app.width = 1079
        wait(25)
        compare(app.sidebarVisible, true)
        app.width = 1080
        wait(25)
        compare(app.sidebarVisible, false)
        const search = findChild(app, "searchField")
        keyClick(Qt.Key_F, Qt.ControlModifier)
        tryCompare(search, "activeFocus", true)
        search.text = "cached"
        mock.search = "cached"
        keyClick(Qt.Key_Escape)
        compare(search.text, "")
        compare(app.sidebarExpanded, true) // Hidden compact state must not consume Escape.
    }
    function test_shell_planes_and_window_chrome_are_stable() {
        const planes = findChild(app, "chromePlanes")
        const sidebarPlane = findChild(app, "sidebarPlane")
        const contentPlane = findChild(app, "contentPlane")
        const backdrop = findChild(app, "chromeBackdrop")
        const chrome = findChild(app, "windowChrome")
        const controls = findChild(app, "windowControls")
        verify(planes !== null)
        verify(sidebarPlane !== null)
        verify(contentPlane !== null)
        verify(backdrop !== null)
        verify(chrome !== null)
        verify(controls !== null)
        compare(backdrop.sourceItem, planes)
        verify(backdrop.sourceItem !== findChild(app, "mailScene"))
        compare(App.MailStyle.shellHeaderHeight, 35)
        compare(App.MailStyle.toolbarButtonHeight, 26)
        compare(chrome.height, 35)
        compare(chrome.width, app.width)
        verify(chrome.visible)
        verify(controls.width > 0 && controls.height > 0)
        compare(sidebarPlane.width, app.sidebarWidth)
        compare(contentPlane.x, app.sidebarWidth)
        verify(contentPlane.width > 0)
        const header = findChild(app, "floatingHeader")
        const sidebar = findChild(app, "sidebarBody")
        const content = findChild(app, "contentScene")
        const scene = findChild(app, "mailScene")
        compare(header.parent, scene)
        compare(sidebar.parent, scene)
        compare(content.parent, scene)
        compare(header.y, 0)
        compare(header.height, 35)
        compare(sidebar.y, 35)
        compare(content.y, 35)
        verify(sidebar.height > 0 && content.height > 0)
        const listHeader = findChild(app, "listHeader")
        const readerHeader = findChild(app, "readerHeader")
        const title = findChild(app, "folderTitle")
        verify(isWithin(title, listHeader))
        compare(readerHeader.visible, true)
        for (const name of ["mailboxActionGroup", "unreadFilterButton", "listOptionsButton", "toolbarSearch", "searchToggleButton", "searchField", "readerBackButton", "replyActionGroup", "detailsButton", "moreButton"]) {
            const item = findChild(app, name)
            verify(item !== null)
            verify(isWithin(item, readerHeader), name + " must belong to readerHeader")
            verify(!isWithin(item, listHeader), name + " must not belong to the title-only listHeader")
        }
        compare(listHeader.children.length, 1, "listHeader must contain only the folder title")
        compare(listHeader.children[0], title)
        verify(isWithin(findChild(app, "sidebarToggleButton"), header))
        compare(findChild(app, "messageList").mapToItem(scene, 0, 0).y, 35)
        compare(findChild(app, "readerScroll").mapToItem(scene, 0, 0).y, 35)
    }

    function test_shell_geometry_at_width_boundaries() {
        const scene = findChild(app, "mailScene")
        const content = findChild(app, "contentScene")
        const header = findChild(app, "floatingHeader")
        const sidebar = findChild(app, "sidebarBody")
        const list = findChild(app, "messagePane")
        const reader = findChild(app, "messageReader")
        const controls = findChild(app, "windowControls")
        const listHeader = findChild(app, "listHeader")
        const readerHeader = findChild(app, "readerHeader")
        compare(findChild(app, "statusStrip"), null)
        for (const width of [580, 650, 799, 800, 820, 1079, 1080, 1280]) {
            for (const expanded of [false, true]) {
                for (const reading of [false, true]) {
                    app.width = width
                    app.sidebarExpanded = expanded
                    app.sidebarCollapsed = !expanded
                    app.sidebarPreferredWidth = 340
                    app.readerVisible = reading
                    wait(25)
                    compare(header.width, width)
                    compare(app.sidebarVisible, expanded)
                    const toggle = findChild(app, "sidebarToggleButton")
                    verify(toggle.visible)
                    compare(toggle.mapToItem(scene, 0, 0).x, expanded ? app.sidebarWidth - 38 : 14)
                    for (const name of ["folderTitle", "mailboxActionGroup", "unreadFilterButton", "listOptionsButton", "searchToggleButton", "readerBackButton", "windowControls"])
                        verifySeparate(toggle, findChild(app, name))
                    compare(findChild(app, "toolbarSearch").active, false)
                    compare(content.x, app.sidebarVisible ? app.sidebarWidth : 0)
                    compare(content.y, 35)
                    compare(content.y + content.height, scene.height)
                    compare(sidebar.y + sidebar.height, scene.height)
                    compare(list.visible, !app.narrow || !reading)
                    compare(reader.visible, !app.narrow || reading)
                    compare(listHeader.visible, list.visible)
                    compare(listHeader.x, list.mapToItem(scene, 0, 0).x)
                    compare(listHeader.width, list.width)
                    compare(readerHeader.visible, true)
                    compare(readerHeader.x, app.narrow ? content.x : reader.mapToItem(scene, 0, 0).x)
                    compare(readerHeader.width, app.narrow ? content.width : reader.width)
                    verify(readerHeader.width >= 260)
                    const controlTop = controls.mapToItem(scene, 0, 0)
                    verify(controlTop.x >= 0 && controlTop.x + controls.width <= width)
                    compare(controlTop.y + controls.height / 2, header.height / 2)
                    const leading = app.narrow && !expanded ? 48 : 14
                    const trailing = Math.floor(Math.min(readerHeader.width - 14,
                        controlTop.x - readerHeader.x - 12))
                    const rowWidth = trailing - leading
                    const titleWidth = app.narrow && !reading ? Math.min(120, Math.max(48, Math.floor(rowWidth * .25))) : 0
                    const actionWidth = Math.max(0, rowWidth - titleWidth - (titleWidth > 0 ? 8 : 0))
                    const messageActions = !app.narrow || reading
                    const backVisible = app.narrow && reading
                    const baselineWidth = 60 + (messageActions ? 34 : 0) + (backVisible ? 34 : 0)
                    compare(readerHeader.leadingInset, leading)
                    compare(readerHeader.trailingEdge, trailing)
                    compare(readerHeader.rowWidth, rowWidth)
                    compare(readerHeader.titleWidth, titleWidth)
                    compare(readerHeader.actionWidth, actionWidth)
                    compare(readerHeader.actionLeft, leading + titleWidth + (titleWidth > 0 ? 8 : 0))
                    compare(readerHeader.messageActions, messageActions)
                    compare(readerHeader.backVisible, backVisible)
                    compare(readerHeader.baselineWidth, baselineWidth)
                    compare(readerHeader.compactActions, actionWidth < baselineWidth + 60)
                    compare(readerHeader.showReplyGroup, messageActions && actionWidth >= baselineWidth + 120)
                    compare(readerHeader.fullReplyGroup, messageActions && actionWidth >= baselineWidth + 146)
                    compare(readerHeader.directDetails, messageActions && actionWidth >= baselineWidth + 180)
                    compare(readerHeader.directFilter, actionWidth >= baselineWidth + 60 + (messageActions ? 120 : 0) + 34)
                    compare(findChild(app, "mailboxActionGroup").visible, !readerHeader.compactActions)
                    compare(findChild(app, "unreadFilterButton").visible, readerHeader.directFilter)
                    compare(findChild(app, "listOptionsButton").visible, true)
                    compare(findChild(app, "searchToggleButton").visible, true)
                    compare(findChild(app, "replyActionGroup").visible, readerHeader.showReplyGroup)
                    compare(findChild(app, "replyActionGroup").icons.length, readerHeader.fullReplyGroup ? 3 : 2)
                    compare(findChild(app, "detailsButton").visible, readerHeader.directDetails)
                    compare(findChild(app, "moreButton").visible, messageActions)
                    compare(findChild(app, "readerBackButton").visible, backVisible)
                    const title = findChild(app, "folderTitle")
                    compare(title.visible, list.visible)
                    compare(title.x, expanded ? 14 : 48)
                    if (app.narrow && !reading) compare(title.width, titleWidth)
                    if (!app.narrow) compare(title.width, listHeader.width - title.x - 14)
                    if (width === 580 && expanded) {
                        compare(readerHeader.compactActions, true)
                        compare(findChild(app, "mailboxActionGroup").visible, false)
                        compare(findChild(app, "unreadFilterButton").visible, false)
                        compare(findChild(app, "listOptionsButton").visible, true)
                        if (reading) {
                            compare(readerHeader.showReplyGroup, false)
                            compare(readerHeader.fullReplyGroup, false)
                            compare(findChild(app, "replyActionGroup").visible, false)
                            compare(findChild(app, "detailsButton").visible, false)
                            compare(findChild(app, "moreButton").visible, true)
                        }
                    }
                    const names = ["sidebarToggleButton", "folderTitle", "mailboxActionGroup", "unreadFilterButton", "listOptionsButton", "searchToggleButton", "searchField", "readerBackButton", "replyActionGroup", "detailsButton", "moreButton"]
                    for (const name of names) {
                        const item = findChild(app, name)
                        if (!item.visible) continue
                        const top = item.mapToItem(scene, 0, 0)
                        verify(item.width > 0 && item.height > 0, name)
                        verify(top.x >= 0 && top.x + item.width <= width, name + " outside window")
                        verify(top.y >= 0 && top.y + item.height <= header.height, name + " outside header")
                        if (name !== "sidebarToggleButton" && name !== "folderTitle") {
                            verify(top.x >= readerHeader.x + readerHeader.actionLeft,
                                name + " starts outside the reader action budget")
                            verify(top.x + item.width <= readerHeader.x + readerHeader.trailingEdge,
                                name + " ends outside the reader action budget")
                            if (!app.narrow) {
                                const readerTop = reader.mapToItem(scene, 0, 0)
                                verify(top.x >= readerTop.x && top.x + item.width <= readerTop.x + reader.width,
                                    name + " must stay entirely above the reader, not the list")
                            }
                        }
                        compare(top.y + item.height / 2, controlTop.y + controls.height / 2, name + " center line")
                        verify(top.x + item.width <= controlTop.x - 12, name + " overlaps window controls: " + width + "/" + expanded + "/" + reading + " right=" + (top.x + item.width) + " limit=" + (controlTop.x - 12))
                        for (const other of names) {
                            if (name !== other) verifySeparate(item, findChild(app, other))
                        }
                    }
                    for (const name of ["sidebarDivider", "messageDivider"]) {
                        const divider = findChild(app, name)
                        if (!divider.visible) continue
                        compare(divider.y, 35)
                        compare(divider.y + divider.height, scene.height)
                    }
                }
            }
        }
    }

    function test_default_reader_budget_shows_all_direct_commands() {
        const reader = findChild(app, "messageReader")
        const header = findChild(app, "readerHeader")
        compare(app.width, 1280)
        compare(reader.width, 724)
        compare(header.actionWidth, 613)
        compare(header.baselineWidth, 94)
        compare(header.compactActions, false)
        compare(header.showReplyGroup, true)
        compare(header.fullReplyGroup, true)
        compare(header.directDetails, true)
        compare(header.directFilter, true)
        for (const name of ["mailboxActionGroup", "unreadFilterButton", "listOptionsButton", "searchToggleButton", "replyActionGroup", "detailsButton", "moreButton"])
            compare(findChild(app, name).visible, true, name)
        compare(findChild(app, "readerBackButton").visible, false)
    }

    function test_compact_reader_details_anchor_and_search_return() {
        activateApp()
        app.width = 580
        app.sidebarExpanded = true
        app.sidebarPreferredWidth = 340
        app.readerVisible = true
        wait(30)
        compare(findChild(app, "detailsButton").visible, false)
        compare(findChild(app, "readerHeader").showReplyGroup, false)
        compare(findChild(app, "readerHeader").fullReplyGroup, false)
        compare(findChild(app, "replyActionGroup").visible, false)
        compare(findChild(app, "moreButton").visible, true)
        app.showReview("details")
        const details = findChild(app, "messageDetailsPopover")
        tryCompare(details, "visible", true)
        compare(details.anchorItem, findChild(app, "moreButton"))
        details.close()
        keyClick(Qt.Key_F, Qt.ControlModifier)
        compare(app.readerVisible, false)
        const search = findChild(app, "searchField")
        tryCompare(search, "activeFocus", true)
        compare(search.visible, true)
    }

    function test_header_controls_receive_pointer_input() {
        activateApp()
        const scene = findChild(app, "mailScene")
        const refresh = groupButton(findChild(app, "mailboxActionGroup"), 0)
        const point = refresh.mapToItem(scene, refresh.width / 2, refresh.height / 2)
        mouseClick(scene, point.x, point.y)
        compare(mock.refreshCount, 1)
        clickInScene(findChild(app, "searchToggleButton"))
        const search = findChild(app, "searchField")
        tryCompare(search, "activeFocus", true)
        const input = search.mapToItem(scene, search.width / 2, search.height / 2)
        mouseClick(scene, input.x, input.y)
        tryCompare(search, "activeFocus", true)
        keyClick(Qt.Key_A)
        tryCompare(mock, "search", "a")
        compare(findChild(app, "sidebarComposeButton"), null)
        clickInScene(findChild(app, "searchToggleButton"))
        tryCompare(findChild(app, "mailboxActionGroup"), "visible", true)
        const compose = groupButton(findChild(app, "mailboxActionGroup"), 1)
        const composePoint = compose.mapToItem(scene, compose.width / 2, compose.height / 2)
        mouseClick(scene, composePoint.x, composePoint.y)
        tryCompare(findChild(app, "composerDialog"), "visible", true)
    }

    function test_toolbar_groups_preserve_command_semantics() {
        activateApp()
        const mailbox = findChild(app, "mailboxActionGroup")
        const replies = findChild(app, "replyActionGroup")
        compare(mailbox.icons.length, 2)
        compare(replies.icons.length, 3)
        compare(replies.labels[1], "回复全部")
        compare(replies.icons[0], "undo")
        compare(replies.icons[1], "arrows")
        compare(replies.icons[2], "forward")
        for (const group of [mailbox, replies]) {
            compare(group.width, group.icons.length * 26)
            compare(group.height, 26)
            compare(group.materialForm, true)
            compare(group.backdrop, findChild(app, "chromeBackdrop"))
            for (let index = 0; index < group.icons.length; ++index) {
                const button = groupButton(group, index)
                compare(button.iconOnly, true)
                compare(button.Accessible.name, group.labels[index])
                compare(button.checkable, false)
                compare(button.checked, false)
            }
        }
        const refresh = groupButton(mailbox, 0)
        mock.busy = true
        compare(refresh.enabled, false)
        clickInScene(refresh)
        compare(mock.refreshCount, 0)
        mock.busy = false
        refresh.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(mock.refreshCount, 1)
        compare(refresh.checked, false)
        mock.demo = false; mock.configured = false
        compare(groupButton(mailbox, 1).enabled, false)
        mock.demo = true; mock.configured = true
        const composer = findChild(app, "composerDialog")
        const modes = ["reply", "replyAll", "forward"]
        for (let index = 0; index < modes.length; ++index) {
            clickInScene(groupButton(replies, index))
            tryCompare(composer, "visible", true)
            compare(mock.lastComposeMode, modes[index])
            keyClick(Qt.Key_Escape)
            tryCompare(composer, "visible", false)
        }
        groupButton(replies, 1).forceActiveFocus()
        keyClick(Qt.Key_Space)
        tryCompare(composer, "visible", true)
        compare(mock.lastComposeMode, "replyAll")
        keyClick(Qt.Key_Escape)
        tryCompare(composer, "visible", false)
        mock.sending = true
        for (let index = 0; index < modes.length; ++index) compare(groupButton(replies, index).enabled, false)
        mock.sending = false; mock.selectedUid = ""
        for (let index = 0; index < modes.length; ++index) compare(groupButton(replies, index).enabled, false)
    }
    function test_list_toolbar_icon_labels_and_canonical_hover_tooltips() {
        activateApp()
        const scene = findChild(app, "mailScene")
        const unread = findChild(app, "unreadFilterButton")
        const options = findChild(app, "listOptionsButton")
        compare(unread.iconName, "circle")
        compare(options.iconName, "grid")
        compare(unread.checkable, true)
        compare(options.checkable, false)
        for (const item of [unread, options]) {
            compare(item.iconOnly, true)
            compare(item.text, "")
            compare(item.width, 26)
            compare(item.height, 26)
            compare(item.backdrop, findChild(app, "chromeBackdrop"))
            compare(item.Accessible.name, item === unread ? "仅显示已缓存的未读邮件" : "列表显示选项")
            const tooltip = findChild(item, "kosToolTip")
            verify(tooltip !== null)
            compare(tooltip.target, item)
            compare(tooltip.text, item.Accessible.name)
            // Hover is real input; do not make the tooltip visible directly.
            const point = item.mapToItem(scene, item.width / 2, item.height / 2)
            mouseMove(scene, point.x, point.y)
            tryCompare(item, "hovered", true)
            tryCompare(tooltip, "visible", true)
            compare(tooltip.text, item.Accessible.name)
            mouseMove(scene, scene.width / 2, 100)
            tryCompare(tooltip, "visible", false)
        }
        mock.unreadOnly = true
        compare(unread.Accessible.name, "显示全部已缓存邮件")
        compare(findChild(unread, "kosToolTip").text, "显示全部已缓存邮件")
    }
    function test_unread_filter_pointer_space_and_menu_stay_synchronized() {
        activateApp()
        const unread = findChild(app, "unreadFilterButton")
        const options = findChild(app, "listOptionsButton")
        const popover = findChild(app, "listOptionsPopover")
        const check = findChild(app, "unreadFilterCheck")
        compare(mock.unreadOnly, false)
        compare(unread.checked, false)
        compare(check.checked, false)
        clickInScene(unread)
        compare(mock.unreadOnly, true)
        compare(unread.checked, true)
        compare(unread.highlighted, true)
        compare(options.highlighted, true)
        compare(check.checked, true)
        compare(unread.Accessible.name, "显示全部已缓存邮件")
        clickInScene(unread)
        compare(mock.unreadOnly, false)
        compare(unread.checked, false)
        unread.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(mock.unreadOnly, true)
        compare(check.checked, true)
        keyClick(Qt.Key_Space)
        compare(mock.unreadOnly, false)
        clickInScene(options)
        tryCompare(popover, "visible", true)
        compare(popover.anchorItem, options)
        compare(check.text, "仅显示未读邮件（已缓存）")
        clickInScene(check)
        compare(mock.unreadOnly, true)
        compare(unread.checked, true)
        compare(check.checked, true)
        compare(popover.visible, true)
        check.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(mock.unreadOnly, false)
        compare(unread.checked, false)
        compare(check.checked, false)
        // Repeated native button/check-box toggles must not replace bindings.
        for (const value of [true, false, true, false]) {
            mock.unreadOnly = value
            compare(unread.checked, value)
            compare(unread.highlighted, value)
            compare(options.highlighted, value)
            compare(check.checked, value)
            compare(unread.Accessible.name, value ? "显示全部已缓存邮件" : "仅显示已缓存的未读邮件")
        }
        keyClick(Qt.Key_Escape)
        tryCompare(popover, "visible", false)
        clickInScene(unread)
        compare(mock.unreadOnly, true)
        options.forceActiveFocus()
        keyClick(Qt.Key_Space)
        tryCompare(popover, "visible", true)
        compare(check.checked, true)
    }
    function test_summary_menu_changes_only_list_preview_and_accessible_summary() {
        activateApp()
        const list = findChild(app, "messageList")
        tryVerify(() => list.itemAtIndex(0) !== null)
        const preview = findChild(list.itemAtIndex(0), "messagePreview")
        const body = findChild(app, "messageBody")
        const reader = findChild(app, "messageReader")
        const check = findChild(app, "summaryCheck")
        const popover = findChild(app, "listOptionsPopover")
        const message = JSON.stringify(mock.selectedMessage)
        const originalBody = body.text
        const selectedUid = mock.selectedUid
        verify(preview !== null)
        compare(app.showSummary, true)
        compare(preview.visible, true)
        verify(list.itemAtIndex(0).Accessible.name.indexOf("A synthetic message.") >= 0)
        clickInScene(findChild(app, "listOptionsButton"))
        tryCompare(popover, "visible", true)
        compare(check.text, "显示邮件摘要")
        compare(check.checked, true)
        clickInScene(check)
        compare(app.showSummary, false)
        compare(preview.visible, false)
        compare(popover.visible, true)
        verify(list.itemAtIndex(0).Accessible.name.indexOf("A synthetic message.") < 0)
        check.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(app.showSummary, true)
        compare(preview.visible, true)
        verify(list.itemAtIndex(0).Accessible.name.indexOf("A synthetic message.") >= 0)
        for (const value of [false, true, false, true]) {
            app.showSummary = value
            compare(check.checked, value)
            compare(preview.visible, value)
        }
        compare(body.text, originalBody)
        compare(body.textFormat, TextEdit.PlainText)
        compare(reader.visible, true)
        compare(mock.selectedUid, selectedUid)
        compare(JSON.stringify(mock.selectedMessage), message)
        compare(mock.unreadOnly, false)
        compare(mock.search, "")
        compare(list.count, 1)
    }
    function test_compact_list_options_overflow_commands_and_disabled_states() {
        activateApp()
        app.width = 580
        app.sidebarPreferredWidth = 340
        app.sidebarExpanded = true
        wait(25)
        const options = findChild(app, "listOptionsButton")
        const popover = findChild(app, "listOptionsPopover")
        const refresh = findChild(app, "overflowRefresh")
        const compose = findChild(app, "overflowCompose")
        const composer = findChild(app, "composerDialog")
        compare(findChild(app, "readerHeader").compactActions, true)
        compare(findChild(app, "mailboxActionGroup").visible, false)
        compare(findChild(app, "unreadFilterButton").visible, false)
        clickInScene(options)
        tryCompare(popover, "visible", true)
        compare(refresh.visible, true)
        compare(compose.visible, true)
        compare(refresh.text, "收取邮件")
        compare(compose.text, "新建邮件")
        mock.busy = true
        compare(refresh.enabled, false)
        clickInScene(refresh)
        compare(mock.refreshCount, 0)
        compare(popover.visible, true)
        mock.busy = false
        clickInScene(refresh)
        compare(mock.refreshCount, 1)
        tryCompare(popover, "visible", false)
        compare(app.sidebarVisible, true)
        clickInScene(options)
        tryCompare(popover, "visible", true)
        mock.demo = false; mock.configured = false
        compare(refresh.enabled, false)
        compare(compose.enabled, false)
        clickInScene(compose)
        compare(mock.lastComposeMode, "")
        compare(composer.visible, false)
        compare(popover.visible, true)
        mock.configured = true
        compare(refresh.enabled, true)
        compare(compose.enabled, true)
        compose.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(mock.lastComposeMode, "new")
        tryCompare(popover, "visible", false)
        tryCompare(composer, "visible", true)
        keyClick(Qt.Key_Escape)
        tryCompare(composer, "visible", false)
        app.width = 1280
        wait(25)
        clickInScene(options)
        tryCompare(popover, "visible", true)
        compare(refresh.visible, false)
        compare(compose.visible, false)
        compare(findChild(app, "mailboxActionGroup").visible, true)
    }
    function test_list_options_trigger_click_toggles_the_same_popover() {
        activateApp()
        const options = findChild(app, "listOptionsButton")
        const popover = findChild(app, "listOptionsPopover")
        clickInScene(options)
        tryCompare(popover, "visible", true)
        clickInScene(options)
        tryCompare(popover, "visible", false)
        compare(mock.unreadOnly, false)
        compare(app.showSummary, true)
    }
    function test_list_overflow_resize_closes_before_focused_command_disappears() {
        activateApp()
        app.width = 580
        app.sidebarPreferredWidth = 340
        app.sidebarExpanded = true
        wait(25)
        const options = findChild(app, "listOptionsButton")
        const popover = findChild(app, "listOptionsPopover")
        const compose = findChild(app, "overflowCompose")
        compare(findChild(app, "readerHeader").compactActions, true)
        clickInScene(options)
        tryCompare(popover, "visible", true)
        compose.forceActiveFocus()
        tryCompare(compose, "activeFocus", true)
        app.sidebarExpanded = false
        tryCompare(findChild(app, "readerHeader"), "compactActions", false)
        tryCompare(popover, "visible", false)
        compare(compose.activeFocus, false)
        compare(compose.visible, false)
        verify(app.activeFocusItem !== null)
        compare(mock.lastComposeMode, "")
        keyClick(Qt.Key_Space)
        compare(findChild(app, "composerDialog").visible, false)
        compare(mock.lastComposeMode, "")
    }
    function test_list_options_escape_preserves_sidebar_and_reader_state() {
        activateApp()
        const scene = findChild(app, "mailScene")
        const options = findChild(app, "listOptionsButton")
        const popover = findChild(app, "listOptionsPopover")
        for (const width of [1280, 650]) {
            app.width = width
            app.sidebarExpanded = true
            app.sidebarCollapsed = false
            app.readerVisible = width === 1280
            wait(25)
            const expanded = app.sidebarExpanded
            const collapsed = app.sidebarCollapsed
            const reading = app.readerVisible
            clickInScene(options)
            tryCompare(popover, "visible", true)
            mouseMove(scene, scene.width / 2, scene.height - 30)
            keyClick(Qt.Key_Escape)
            tryCompare(popover, "visible", false)
            compare(app.sidebarExpanded, expanded)
            compare(app.sidebarCollapsed, collapsed)
            compare(app.sidebarVisible, true)
            compare(app.readerVisible, reading)
        }
    }
    function test_search_drafts_and_reader_transition_close_options() {
        activateApp()
        const options = findChild(app, "listOptionsButton")
        const unread = findChild(app, "unreadFilterButton")
        const popover = findChild(app, "listOptionsPopover")
        const search = findChild(app, "searchField")
        clickInScene(options)
        tryCompare(popover, "visible", true)
        keyClick(Qt.Key_F, Qt.ControlModifier)
        tryCompare(findChild(app, "toolbarSearch"), "active", true)
        tryCompare(search, "activeFocus", true)
        compare(popover.visible, false)
        compare(options.visible, false)
        compare(unread.visible, false)
        keyClick(Qt.Key_Escape)
        tryCompare(options, "visible", true)
        clickInScene(options)
        tryCompare(popover, "visible", true)
        app.draftsVisible = true
        tryCompare(popover, "visible", false)
        compare(options.visible, false)
        compare(unread.visible, false)
        compare(findChild(app, "mailboxActionGroup").visible, true)
        app.draftsVisible = false
        app.width = 580
        app.sidebarPreferredWidth = 340
        app.sidebarExpanded = true
        wait(25)
        // Drafts retain the two direct commands, even in the compact tier.
        app.draftsVisible = true
        compare(findChild(app, "mailboxActionGroup").visible, true)
        app.draftsVisible = false
        compare(findChild(app, "mailboxActionGroup").visible, false)
        clickInScene(options)
        tryCompare(popover, "visible", true)
        app.readerVisible = true
        tryCompare(popover, "visible", false)
        compare(findChild(app, "listHeader").visible, false)
        compare(findChild(app, "readerHeader").visible, true)
        compare(options.visible, true)
        compare(unread.visible, false)
        clickInScene(options)
        tryCompare(popover, "visible", true)
    }
    function test_reader_transition_closes_popups_without_closing_search() {
        activateApp()
        app.width = 650
        app.sidebarExpanded = false
        wait(25)
        for (const pair of [["listOptionsButton", "listOptionsPopover"], ["moreButton", "moreMessageMenu"], ["detailsButton", "messageDetailsPopover"]]) {
            app.readerVisible = true
            wait(25)
            const button = findChild(app, pair[0])
            const popup = findChild(app, pair[1])
            compare(button.visible, true)
            clickInScene(button)
            tryCompare(popup, "visible", true)
            app.readerVisible = false
            tryCompare(popup, "visible", false)
        }
        keyClick(Qt.Key_F, Qt.ControlModifier)
        const search = findChild(app, "searchField")
        const host = findChild(app, "toolbarSearch")
        tryCompare(search, "activeFocus", true)
        search.text = "cached"
        mock.search = "cached"
        for (const reading of [true, false]) {
            app.readerVisible = reading
            wait(25)
            compare(host.active, true)
            compare(search.visible, true)
            compare(search.activeFocus, true)
            compare(search.text, "cached")
            compare(mock.search, "cached")
            compare(findChild(app, "readerHeader").visible, true)
        }
    }

    function test_drafts_keep_mailbox_group_and_hide_message_commands() {
        for (const width of [580, 1280]) {
            app.width = width
            app.sidebarPreferredWidth = 340
            app.sidebarExpanded = true
            app.sidebarCollapsed = false
            app.draftsVisible = true
            for (const reading of [false, true]) {
                app.readerVisible = reading
                wait(25)
                const header = findChild(app, "readerHeader")
                compare(header.visible, true)
                compare(header.baselineWidth, 52)
                compare(header.messageActions, false)
                compare(header.compactActions, false)
                compare(header.showReplyGroup, false)
                compare(header.fullReplyGroup, false)
                compare(header.directDetails, false)
                compare(header.directFilter, false)
                const mailbox = findChild(app, "mailboxActionGroup")
                compare(mailbox.visible, true)
                compare(groupButton(mailbox, 0).enabled, false)
                compare(groupButton(mailbox, 1).enabled, true)
                for (const name of ["replyActionGroup", "detailsButton", "moreButton", "unreadFilterButton", "listOptionsButton", "searchToggleButton"])
                    compare(findChild(app, name).visible, false, name + " must hide in drafts")
                const title = findChild(app, "folderTitle")
                compare(title.visible, !app.narrow || !reading)
                if (title.visible) compare(title.text, "草稿")
            }
        }
    }

    function test_narrow_reply_commands_are_available_in_more_menu() {
        activateApp()
        app.width = 580
        app.sidebarPreferredWidth = 340
        app.sidebarExpanded = true
        app.readerVisible = true
        wait(25)
        const replies = findChild(app, "replyActionGroup")
        const more = findChild(app, "moreButton")
        const menu = findChild(app, "moreMessageMenu")
        const composer = findChild(app, "composerDialog")
        compare(findChild(app, "readerHeader").fullReplyGroup, false)
        compare(replies.visible, false)
        compare(more.visible, true)
        clickInScene(more)
        tryCompare(menu, "visible", true)
        const reply = moreCommand("undo")
        const replyAll = moreCommand("arrows")
        const forward = moreCommand("forward")
        compare(reply.text, "回复")
        compare(replyAll.text, "回复全部")
        compare(forward.text, "转发")
        for (const command of [reply, replyAll, forward]) {
            mock.sending = false
            compare(command.visible, true)
            compare(command.enabled, true)
            clickInScene(command)
            tryCompare(menu, "visible", false)
            tryCompare(composer, "visible", true)
            compare(mock.lastComposeMode, command === reply ? "reply" : command === replyAll ? "replyAll" : "forward")
            keyClick(Qt.Key_Escape)
            tryCompare(composer, "visible", false)
            clickInScene(more)
            tryCompare(menu, "visible", true)
        }
        mock.sending = true
        compare(replyAll.enabled, false)
        clickInScene(replyAll)
        compare(composer.visible, false)
        compare(menu.visible, true)
        mock.sending = false
        compare(replyAll.enabled, true)
        clickInScene(replyAll)
        tryCompare(menu, "visible", false)
        tryCompare(composer, "visible", true)
        compare(mock.lastComposeMode, "replyAll")
    }
    function test_toolbar_tier_resize_moves_focus_to_surviving_commands() {
        activateApp()
        const unread = findChild(app, "unreadFilterButton")
        const options = findChild(app, "listOptionsButton")
        const header = findChild(app, "readerHeader")
        // Increasing the list now shrinks the reader's action budget, not the list toolbar.
        app.width = 1080
        app.listPreferredWidth = 260
        wait(25)
        compare(unread.visible, true)
        unread.forceActiveFocus()
        tryCompare(unread, "activeFocus", true)
        app.listPreferredWidth = 520
        tryCompare(header, "directFilter", false)
        tryCompare(unread, "visible", false)
        tryCompare(options, "activeFocus", true)
        app.width = 580
        app.sidebarPreferredWidth = 340
        app.sidebarExpanded = false
        wait(25)
        const mailbox = findChild(app, "mailboxActionGroup")
        compare(mailbox.visible, true)
        groupButton(mailbox, 0).forceActiveFocus()
        tryVerify(() => app.focusWithin(mailbox))
        app.sidebarExpanded = true
        tryCompare(mailbox, "visible", false)
        tryCompare(options, "activeFocus", true)
        app.width = 650
        app.sidebarPreferredWidth = 236
        app.readerVisible = true
        wait(25)
        const replies = findChild(app, "replyActionGroup")
        const more = findChild(app, "moreButton")
        compare(replies.visible, true)
        compare(replies.icons.length, 3)
        groupButton(replies, 1).forceActiveFocus()
        tryVerify(() => app.focusWithin(replies))
        app.sidebarPreferredWidth = 290
        tryCompare(header, "fullReplyGroup", false)
        tryCompare(header, "showReplyGroup", true)
        tryCompare(replies, "width", 52)
        tryCompare(more, "activeFocus", true)
        compare(replies.icons.length, 2)
        groupButton(replies, 0).forceActiveFocus()
        tryVerify(() => app.focusWithin(replies))
        app.width = 580
        app.sidebarPreferredWidth = 340
        tryCompare(header, "showReplyGroup", false)
        tryCompare(replies, "visible", false)
        tryCompare(more, "activeFocus", true)
    }
    function test_search_expands_on_the_traffic_light_row_and_closes_safely() {
        activateApp()
        const toggle = findChild(app, "searchToggleButton")
        const search = findChild(app, "searchField")
        const host = findChild(app, "toolbarSearch")
        const group = findChild(app, "mailboxActionGroup")
        const controls = findChild(app, "windowControls")
        const scene = findChild(app, "mailScene")
        clickInScene(toggle)
        tryCompare(search, "activeFocus", true)
        compare(host.active, true)
        compare(group.visible, false)
        compare(findChild(app, "folderTitle").visible, true)
        verifySeparate(findChild(app, "folderTitle"), search)
        compare(search.height, 26)
        compare(search.materialForm, true)
        compare(search.liquidFinish, true)
        compare(search.backdrop, findChild(app, "chromeBackdrop"))
        compare(search.Accessible.name, "搜索已缓存的邮件")
        compare(search.mapToItem(scene, 0, search.height / 2).y, controls.mapToItem(scene, 0, controls.height / 2).y)
        verifySeparate(search, toggle)
        keyClick(Qt.Key_A)
        tryCompare(mock, "search", "a")
        findChild(app, "sidebarToggleButton").forceActiveFocus()
        compare(host.active, true)
        compare(mock.search, "a")
        clickInScene(toggle)
        compare(host.active, false)
        compare(search.text, "")
        compare(mock.search, "")
        tryCompare(group, "visible", true)
        clickInScene(toggle)
        tryCompare(search, "activeFocus", true)
        keyClick(Qt.Key_B)
        keyClick(Qt.Key_Escape)
        compare(host.active, false)
        wait(250)
        compare(mock.search, "")
        compare(search.text, "")
        clickInScene(toggle)
        tryCompare(search, "activeFocus", true)
        keyClick(Qt.Key_C)
        app.draftsVisible = true
        compare(toggle.visible, false)
        compare(host.active, false)
        compare(mock.search, "")
        tryCompare(group, "visible", true)
        verify(findChild(app, "folderTitle").visible)
        keyClick(Qt.Key_F, Qt.ControlModifier)
        tryCompare(search, "activeFocus", true)
        compare(app.draftsVisible, false)
        app.width = 580
        app.sidebarExpanded = true
        app.readerVisible = true
        wait(25)
        keyClick(Qt.Key_F, Qt.ControlModifier)
        tryCompare(search, "activeFocus", true)
        compare(app.sidebarVisible, false)
        compare(app.readerVisible, false)
        tryVerify(() => search.width >= 200)
    }
    function test_search_animation_and_material_fallbacks() {
        activateApp()
        AppTheme.reduceMotion = false
        const toggle = findChild(app, "searchToggleButton")
        const search = findChild(app, "searchField")
        const group = findChild(app, "mailboxActionGroup")
        clickInScene(toggle)
        compare(group.visible, false)
        tryCompare(search, "activeFocus", true)
        for (const mode of ["auto", "solid"]) {
            AppTheme.materialMode = mode
            AppTheme.reduceTransparency = mode === "auto"
            compare(search.materialForm, true)
            compare(findChild(app, "replyActionGroup").materialForm, true)
        }
        clickInScene(toggle)
        tryCompare(group, "visible", true)
        compare(search.visible, false)
    }
    function test_expanded_search_geometry_at_width_boundaries() {
        activateApp()
        const scene = findChild(app, "mailScene")
        const search = findChild(app, "searchField")
        const controls = findChild(app, "windowControls")
        const readerHeader = findChild(app, "readerHeader")
        const host = findChild(app, "toolbarSearch")
        const title = findChild(app, "folderTitle")
        const names = ["sidebarToggleButton", "folderTitle", "mailboxActionGroup", "unreadFilterButton", "listOptionsButton", "searchToggleButton", "searchField", "readerBackButton", "replyActionGroup", "detailsButton", "moreButton"]
        for (const width of [580, 650, 799, 800, 820, 1079, 1080, 1280]) {
            app.width = width
            wait(25)
            keyClick(Qt.Key_F, Qt.ControlModifier)
            tryCompare(search, "activeFocus", true)
            for (const expanded of [false, true]) {
                app.sidebarExpanded = expanded
                app.sidebarCollapsed = !expanded
                app.sidebarPreferredWidth = 340
                for (const reading of [false, true]) {
                    app.readerVisible = reading
                    wait(25)
                    compare(readerHeader.visible, true)
                    compare(host.active, true) // Changing readers must not dismiss or orphan search.
                    compare(search.activeFocus, true)
                    compare(title.visible, !app.narrow || !reading)
                    if (title.visible) {
                        compare(title.text, "收件箱")
                        verify(title.width > 0)
                        if (app.narrow) compare(title.width, readerHeader.titleWidth)
                    }
                    verify(search.width > 0 && search.width <= 280)
                    const lights = controls.mapToItem(scene, 0, 0)
                    for (const name of names) {
                        const item = findChild(app, name)
                        if (!item.visible) continue
                        const top = item.mapToItem(scene, 0, 0)
                        verify(item.width > 0 && item.height > 0, name)
                        compare(top.y + item.height / 2, lights.y + controls.height / 2, name + " center line")
                        verify(top.x >= 0 && top.x + item.width <= lights.x - 12,
                            name + " overlaps window controls during search: " + width + "/" + expanded + "/" + reading)
                        if (name === "searchField" || name === "searchToggleButton") {
                            verify(top.x >= readerHeader.x + readerHeader.actionLeft)
                            verify(top.x + item.width <= readerHeader.x + readerHeader.trailingEdge)
                        }
                        for (const other of names) {
                            if (name !== other) verifySeparate(item, findChild(app, other))
                        }
                    }
                    for (const name of ["mailboxActionGroup", "unreadFilterButton", "listOptionsButton", "readerBackButton", "replyActionGroup", "detailsButton", "moreButton", "listOptionsPopover", "moreMessageMenu", "messageDetailsPopover"])
                        compare(findChild(app, name).visible, false, name + " must hide during search")
                }
            }
        }
    }
    function test_column_widths_are_adjustable_and_clamped() {
        const sidebarDivider = findChild(app, "sidebarDivider")
        const listDivider = findChild(app, "messageDivider")
        compare(app.sidebarWidth, 236)
        compare(app.listWidth, 320)
        sidebarDivider.resizeTo(290)
        listDivider.resizeTo(410)
        compare(app.sidebarPreferredWidth, 290)
        compare(app.listPreferredWidth, 410)
        sidebarDivider.resizeTo(900)
        compare(app.sidebarPreferredWidth, 340)
        listDivider.resizeTo(1)
        compare(app.listPreferredWidth, 260)
        app.width = 820
        verify(app.listWidth <= app.width - 300)
        compare(app.sidebarPreferredWidth, 340)
        app.width = 1280
        compare(app.sidebarWidth, 340)
    }
    function test_column_divider_pointer_drag() {
        const divider = findChild(app, "sidebarDivider")
        const scene = findChild(app, "mailScene")
        // mouseDrag remaps every intermediate point through its target item.
        // The divider itself moves while resizing, so inject into the stable
        // scene coordinate space instead of accidentally chasing the handle.
        const start = divider.mapToItem(scene, divider.width / 2, 120)
        mouseDrag(scene, start.x, start.y, 40, 0)
        compare(app.sidebarPreferredWidth, 276)
        tryCompare(divider, "x", 272)
        const next = divider.mapToItem(scene, divider.width / 2, 120)
        mouseDrag(scene, next.x, next.y, -40, 0)
        compare(app.sidebarPreferredWidth, 236)
    }
    function test_column_divider_keyboard_and_double_click() {
        activateApp()
        const divider = findChild(app, "messageDivider")
        divider.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(app.listPreferredWidth, 330)
        keyClick(Qt.Key_Left)
        compare(app.listPreferredWidth, 320)
        divider.resizeTo(410)
        mouseDoubleClickSequence(divider, divider.width / 2, 120)
        compare(app.listPreferredWidth, 320)
    }
    function test_search_shortcut_and_tab_focus() {
        activateApp()
        app.draftsVisible = true
        keyClick(Qt.Key_F, Qt.ControlModifier)
        const search = findChild(app, "searchField")
        tryCompare(search, "activeFocus", true)
        compare(app.draftsVisible, false)
        for (const character of "example") keyClick(character)
        tryCompare(mock, "search", "example")
        keyClick(Qt.Key_Escape)
        compare(search.text, "")
        compare(mock.search, "")
        keyClick(Qt.Key_Tab)
        compare(search.activeFocus, false)
        verify(app.activeFocusItem !== null)
    }
    function test_escape_preserves_failed_draft_and_returns_from_narrow_reader() {
        activateApp()
        keyClick(Qt.Key_N, Qt.ControlModifier)
        const dialog = findChild(app, "composerDialog")
        tryCompare(dialog, "visible", true)
        const body = findChild(app, "composerBody")
        body.text = "Do not discard on Escape."
        body.forceActiveFocus()
        mock.failSave = true
        keyClick(Qt.Key_Escape)
        compare(dialog.visible, true)
        compare(body.text, "Do not discard on Escape.")
        mock.failSave = false
        keyClick(Qt.Key_Escape)
        tryCompare(dialog, "visible", false)
        compare(mock.lastSaved.body, "Do not discard on Escape.")
        app.width = 650
        app.readerVisible = true
        wait(30)
        keyClick(Qt.Key_Escape)
        compare(app.readerVisible, false)
    }
    function test_three_pane_reader_is_plain_text() {
        const body = findChild(app, "messageBody")
        verify(body !== null)
        compare(body.readOnly, true)
        compare(body.textFormat, TextEdit.PlainText)
        verify(body.text.indexOf("<img") >= 0)
        compare(findChild(app, "messageList").count, 1)
        verify(findChild(app, "messageReader").visible)
    }
    function test_demo_composer_never_sends() {
        app.showReview("compose")
        const dialog = findChild(app, "composerDialog")
        tryCompare(dialog, "visible", true)
        const to = findChild(app, "composerTo")
        to.text = "recipient@example.invalid"
        const send = findChild(app, "sendDraftButton")
        compare(send.enabled, false)
        verify(dialog.finishEditing())
    }
    function test_failed_save_keeps_editor_and_content() {
        app.showReview("compose")
        const dialog = findChild(app, "composerDialog")
        const body = findChild(app, "composerBody")
        body.text = "Keep this local draft."
        mock.failSave = true
        verify(!dialog.finishEditing())
        compare(dialog.visible, true)
        compare(body.text, "Keep this local draft.")
        mock.failSave = false
        verify(dialog.finishEditing())
        compare(mock.lastSaved.body, "Keep this local draft.")
    }
    function test_server_acceptance_only_closes_matching_draft() {
        app.showReview("compose")
        const dialog = findChild(app, "composerDialog")
        mock.sending = true
        verify(!dialog.finishEditing())
        mock.draftAccepted("different-draft")
        compare(dialog.visible, true)
        mock.draftAccepted("test-draft")
        tryCompare(dialog, "visible", false)
    }
    function test_capture_helpers_are_demo_only() {
        mock.demo = false
        app.showReview("account")
        compare(findChild(app, "accountDialog").visible, false)
        app.showReview("compose")
        compare(findChild(app, "composerDialog").visible, false)
    }
    function test_account_password_is_not_prefilled() {
        app.showReview("account")
        compare(findChild(app, "accountPassword").text, "")
        compare(findChild(app, "accountPassword").echoMode, TextInput.Password)
    }
    function test_narrow_window_can_return_to_messages() {
        app.width = 650
        app.readerVisible = true
        wait(30)
        compare(findChild(app, "messageReader").visible, true)
        app.readerVisible = false
        wait(30)
        compare(findChild(app, "messageReader").visible, false)
        compare(findChild(app, "messageList").visible, true)
    }
}

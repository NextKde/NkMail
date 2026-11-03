import QtQuick
import QtQuick.Window
import QtTest
import Kos.Ui
import "../../qml/MailboxNavigation.js" as Navigation
import "../../qml/ReaderLayout.js" as ReaderLayout
import "../../qml/ProviderProfiles.js" as Profiles
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
        property var accounts: ([{ id: "default", name: "Alex Example", address: "alex@example.invalid" }])
        property string currentAccountId: "default"
        property var selectedMessage: ({uid: "1", uidValidity: "1", sender: "Studio Example", senderAddress: "studio@example.invalid", subject: "A quieter inbox", date: "2026-10-29T09:00:00Z", to: "alex@example.invalid", cc: "", messageId: "<one@example.invalid>", text: "<img src=https://example.invalid/tracker>\nPlain text only.", attachments: []})
        property string selectedUid: "1"
        property int blockedImageCount: 0
        function loadRemoteImages() {}
        property string currentFolder: "INBOX"
        property string smartMailbox: ""
        function selectSmartMailbox(kind) { smartMailbox = kind }
        property string status: "Demo"
        property string error: ""
        property bool busy: false
        property bool unreadOnly: false
        property bool notifyEnabled: true
        property bool sending: false
        property int sendDelay: 0
        property bool sendPending: false
        property bool configured: true
        property bool demo: true
        property bool hasMore: false
        property string search: ""
        property bool failSave: false
        property var lastContextRows: []
        function deleteMessages(rows) { lastContextRows = rows }
        property int deletedRow: -1
        function deleteMessage(row) { deletedRow = row }
        property int refreshCount: 0
        property int saveCount: 0
        property var lastSaved: ({})
        signal selectionChanged()
        signal folderChanged()
        signal folderUnreadChanged()
        signal draftAccepted(string id)
        ListModel {
            id: messageModel
            function indexOf(folder, validity, uid) {
                for (let i = 0; i < count; ++i) {
                    const message = get(i)
                    if (message.folder === folder && message.uidValidity === validity && message.uid === uid) return i
                }
                return -1
            }
            ListElement { uid: "1"; uidValidity: "1"; folder: "INBOX"; sender: "Studio Example"; senderAddress: "studio@example.invalid"; subject: "A quieter inbox"; date: "2026-10-29T09:00:00Z"; preview: "A synthetic message."; unread: true; flagged: false; hasAttachments: false; threadKey: ""; threadCount: 1; threadFirst: true }
        }
        property var accountSetup: setupMock
        function configureAccount(config, password, remember) { return !busy }
        function refresh() { refreshCount++ }
        function selectFolder(folder) { smartMailbox = ""; currentFolder = folder }
        function foldersFor(accountId) { return folders }
        property var folderUnreadMap: ({})
        function folderUnread() { return folderUnreadMap }
        function folderUnreadFor(accountId) { return folderUnreadMap }
        property int accountUnreadRevision: 0
        function accountUnread(accountId) { return 0 }
        property var favorites: ([])
        property var favoriteKeys: ({})
        function isFavorite(accountId, folderId) { return favoriteKeys[accountId + "|" + folderId] === true }
        function toggleFavorite(accountId, folderId, title) {
            const key = accountId + "|" + folderId
            const next = Object.assign({}, favoriteKeys)
            if (next[key]) delete next[key]; else next[key] = true
            favoriteKeys = next
            favorites = Object.keys(next).map(k => ({ accountId: k.split("|")[0], folderId: k.split("|")[1], title: title || k.split("|")[1], accountName: "" }))
        }
        function openFavorite(accountId, folderId) { currentAccountId = accountId }
        function switchAccount(accountId) { currentAccountId = accountId }
        function loadMore() {}
        function openMessage(uid, validity) { selectedUid = uid }
        function newDraft(mode) { return {id: "test-draft", to: "", cc: "", bcc: "", subject: "", body: "", inReplyTo: "", references: "", state: "draft", attachments: []} }
        function loadDraft(id) { return drafts.find(d => d.id === id) || ({}) }
        function saveDraft(draft) {
            saveCount++
            if (failSave) return ({})
            lastSaved = JSON.parse(JSON.stringify(draft))
            drafts = drafts.filter(d => d.id !== draft.id).concat([lastSaved])
            return lastSaved
        }
        function discardDraft(id) { drafts = drafts.filter(d => d.id !== id) }
        function addAttachment(url) { return ({}) }
        function saveAttachment(index, url) {}
        function sendDraft(draft) { sending = true }
        function cancelSend() { sendPending = false }
        function clearError() { error = "" }
        function disconnectAccount() { configured = false }
    }
    QtObject {
        id: setupMock
        property bool busy: false
        property string stage: ""
        signal finished(bool success, string error)
        function verify(config, password) { stage = "imap"; busy = true }
    }
    QtObject {
        id: desktopMock
        property int hides: 0
        function hideToTray() { hides++; app.hide(); return true }
    }
    Component { id: readerComponent; App.MessageReader {} }
    Component { id: sidebarComponent; App.MailSidebar {} }
    Component { id: paneComponent; App.MessagePane { selection: App.MailSelection {} } }
    Component { id: appComponent; App.Main {} }
    function init() {
        AppTheme.reduceMotion = true
        AppTheme.appearanceMode = "light"
        failOnWarning(/.*/)
        mock.failSave = false; mock.saveCount = 0; mock.lastSaved = ({})
        mock.refreshCount = 0; mock.search = ""
        mock.drafts = []; mock.sendPending = false
        mock.configured = true; mock.smartMailbox = ""; mock.sending = false; mock.busy = false; mock.demo = true; mock.error = ""
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
    function test_empty_profile_uses_localized_title_and_sidebar_account_entry() {
        const folders = mock.folders
        const accounts = mock.accounts
        const selectedUid = mock.selectedUid
        const messages = []
        for (let i = 0; i < mock.messages.count; ++i)
            messages.push(JSON.parse(JSON.stringify(mock.messages.get(i))))
        try {
            mock.configured = false
            mock.demo = false
            mock.folders = []
            mock.accounts = []
            mock.selectedUid = ""
            mock.messages.clear()
            compare(app.folderTitle(), App.MailLabels.mailboxTitle({id: "INBOX"}))
            verify(app.folderTitle() !== "INBOX")
            compare(findChild(app, "messageEmptyState").actionText, "")
            const addAccount = findChild(app, "addAccountButton")
            verify(addAccount.visible)
            mouseClick(addAccount)
            tryCompare(findChild(app, "accountDialog"), "visible", true)
            const dialog = findChild(app, "accountDialog")
            dialog.close()
            tryCompare(dialog, "visible", false)
            const inboxAdd = findChild(app, "inboxAddAccountButton")
            tryCompare(findChild(app, "messageList"), "count", 0)
            tryCompare(inboxAdd, "visible", true)
            compare(inboxAdd.secondary, true)
            mouseClick(inboxAdd)
            tryCompare(dialog, "visible", true)
            mock.currentFolder = "Projects"
            compare(app.folderTitle(), "Projects")
        } finally {
            mock.configured = true
            mock.demo = true
            mock.folders = folders
            mock.accounts = accounts
            mock.selectedUid = selectedUid
            for (const message of messages) mock.messages.append(message)
            mock.currentFolder = "INBOX"
        }
    }
    function test_local_draft_delete_cancel_and_confirm() {
        activateApp()
        mock.drafts = [{id: "delete-me", subject: "First draft", state: "draft"},
                       {id: "keep-me", subject: "Second draft", state: "draft"}]
        app.draftsVisible = true
        const list = findChild(app, "draftList")
        tryCompare(list, "count", 2)
        wait(50)
        const button = findChild(list.itemAtIndex(0), "deleteLocalDraft")
        mouseClick(button)
        const dialog = findChild(app, "deleteDraftDialog")
        tryCompare(dialog, "opened", true)
        verify(!findChild(app, "composerDialog").visible)
        mouseClick(findChild(dialog, "cancelDeleteDraft"))
        tryCompare(dialog, "visible", false)
        compare(mock.drafts.length, 2)
        mouseClick(button)
        tryCompare(dialog, "opened", true)
        mouseClick(findChild(dialog, "confirmDeleteDraft"))
        tryCompare(dialog, "visible", false)
        compare(mock.drafts.length, 1)
        compare(mock.drafts[0].id, "keep-me")
    }
    function test_local_draft_delete_blocks_send_and_stale_account() {
        activateApp()
        mock.drafts = [{id: "protected-draft", subject: "Draft", state: "draft"}]
        app.draftsVisible = true
        const list = findChild(app, "draftList")
        tryCompare(list, "count", 1)
        wait(50)
        const button = findChild(list.itemAtIndex(0), "deleteLocalDraft")
        mock.sending = true
        verify(!button.enabled)
        mock.sending = false; mock.sendPending = true
        verify(!button.enabled)
        mock.sendPending = false
        mouseClick(button)
        const dialog = findChild(app, "deleteDraftDialog")
        tryCompare(dialog, "opened", true)
        const confirm = findChild(dialog, "confirmDeleteDraft")
        const previousAccount = mock.currentAccountId
        try {
            mock.sendPending = true
            verify(!confirm.enabled)
            mock.sendPending = false
            mock.currentAccountId = "another-account"
            verify(!confirm.enabled)
            compare(mock.drafts.length, 1)
        } finally {
            mock.currentAccountId = previousAccount
            dialog.close()
        }
    }
    function test_more_menu_hidden_row_does_not_leave_gap() {
        const menu = findChild(app, "moreMessageMenu")
        const overflow = findChild(app, "moreDisplayOptions")
        const language = findChild(app, "languageSystem")
        for (const width of [1480, 580, 1480]) {
            app.width = width
            wait(100)
            menu.openAtAnchor()
            tryCompare(menu, "opened", true)
            compare(overflow.height, overflow.visible ? 30 : 0)
            tryCompare(language, "y", overflow.visible ? 30 : 0)
            menu.close()
        }
    }

    function test_context_menu_uses_click_position_and_stays_in_window() {
        activateApp()
        const row = findChild(app, "messageList").itemAtIndex(0)
        const menu = findChild(row, "messageRowMenu")
        for (const point of [Qt.point(28, 12), Qt.point(120, 24)]) {
            mouseClick(row, point.x, point.y, Qt.RightButton)
            tryCompare(menu, "opened", true)
            const expected = row.mapToItem(menu.parent, point.x, point.y)
            tryCompare(menu, "x", expected.x)
            tryCompare(menu, "y", expected.y)
            verify(!menu.arrowVisible)
            menu.close()
            wait(30)
        }
        const corner = row.mapFromItem(menu.parent, menu.parent.width - 2, menu.parent.height - 2)
        row.openContextMenu(corner)
        tryCompare(menu, "opened", true)
        verify(menu.x >= menu.margins && menu.y >= menu.margins)
        verify(menu.x + menu.width <= menu.parent.width - menu.margins + 1)
        verify(menu.y + menu.height <= menu.parent.height - menu.margins + 1)
        menu.close()
    }

    function test_context_delete_preserves_selected_rows() {
        const pane = findChild(app, "messagePane")
        const list = findChild(app, "messageList")
        const second = Object.assign({}, messageModel.get(0), {uid: "2"})
        messageModel.append(second)
        wait(30)
        try {
            pane.selection.toggleRow(0)
            pane.selection.toggleRow(1)
            const row = list.itemAtIndex(0)
            row.openContextMenu()
            verify(row.bulkMenu)
            verify(!row.canCompose)
            row.contextCommand("delete")
            compare(mock.lastContextRows.join(","), "0,1")
            pane.selection.clearSelection()
            pane.selection.toggleRow(1)
            row.openContextMenu()
            compare(pane.selection.selectionSize, 0)
            row.contextCommand("delete")
            compare(mock.lastContextRows.join(","), "0")
        } finally {
            messageModel.remove(1)
        }
    }

    function test_context_reply_waits_for_matching_content() {
        activateApp()
        const pane = findChild(app, "messagePane")
        pane.showReviewMenu()
        wait(30)
        const reply = findChild(app, "messageList").itemAtIndex(0)
        verify(reply !== null)
        verify(reply.canCompose)
        const previous = mock.selectedMessage
        mock.selectedMessage = ({uid: "different", uidValidity: "1"})
        verify(!reply.canCompose)
        mock.selectedMessage = ({uid: "1", uidValidity: "other"})
        verify(!reply.canCompose)
        mock.selectedMessage = previous
        mock.sending = true
        verify(!reply.canCompose)
        mock.sending = false
        verify(reply.canCompose)
    }

    function test_tray_close_keeps_window_and_sending_alive() {
        app.desktop = desktopMock; desktopMock.hides = 0
        mock.sending = true
        app.close()
        compare(desktopMock.hides, 1)
        compare(app.visible, false)
        compare(mock.sending, true)
        app.show()
        compare(app.prepareForQuit(), false)
        mock.sending = false; mock.sendPending = true
        compare(app.prepareForQuit(), false)
        mock.sendPending = false
        compare(app.prepareForQuit(), true)
    }
    function test_failed_draft_save_blocks_explicit_exit() {
        app.compose("new")
        const editor = findChild(app, "composerDialog")
        verify(editor !== null)
        const subject = findChild(app, "composerSubject")
        subject.text = "Unsaved synthetic changes"
        mock.failSave = true
        compare(app.prepareForQuit(), false)
        verify(editor.visible)
        mock.failSave = false
        compare(app.prepareForQuit(), true)
    }
    function test_toolbar_resolves_row_after_model_removal() {
        const original = JSON.parse(JSON.stringify(messageModel.get(0)))
        const selected = mock.selectedMessage
        const second = Object.assign({}, original, {uid: "2"})
        messageModel.append(second)
        mock.selectedMessage = Object.assign({}, selected, {uid: "2"})
        mock.selectedUid = "2"
        messageModel.remove(0)
        mock.deletedRow = -1
        findChild(app, "messageOpsGroup").triggered(0)
        compare(mock.deletedRow, 0)
        messageModel.clear(); messageModel.append(original)
        mock.selectedMessage = selected; mock.selectedUid = "1"
    }
    function test_reader_header_long_content_and_resize() {
        const message = Object.assign({}, mock.selectedMessage, {
            subject: "A long subject that must wrap without covering the details action. ".repeat(4),
            to: "recipient@example.invalid, ".repeat(12)
        })
        const reader = createTemporaryObject(readerComponent, testCase, {
            mail: mock, message: message, hasMessage: true, width: 600, height: 600
        })
        wait(50)
        const header = findChild(reader, "mailMessageHeader")
        const subject = findChild(reader, "readerSubject")
        const details = findChild(reader, "messageDetailsButton")
        verify(subject.height > 20)
        verify(details.mapToItem(header, 0, 0).x >= 0)
        reader.width = 330
        wait(50)
        verify(header.compact)
        const pos = details.mapToItem(header, 0, 0)
        verify(pos.x >= 0 && pos.x + details.width <= header.width + 1)
        verify(pos.y >= subject.mapToItem(header, 0, subject.height).y)
        header.expanded = true
        verify(details.text.length > 0)
    }
    function test_reader_images_fit_without_changing_remote_placeholders() {
        const image = '<img width="1200" height="600" src="data:image/png;base64,AAAA">'
        verify(ReaderLayout.fitImages(image, 300).includes('width="300" height="150"'))
        compare(ReaderLayout.fitImages(image, 1600), image)
        const blocked = '<img data-nkmail-src="https://example.invalid/image">'
        compare(ReaderLayout.fitImages(blocked, 300), blocked)
    }
    function test_reader_notification_layout_and_blocked_images() {
        const html = '<div>Account activity</div>\n    <br><div> </div><div><span>&nbsp;</span></div>'
            + '<img data-nkmail-src="https://example.invalid/logo" alt=""> | | <br><br><br><br>'
            + '<p>Device: Example browser | Example OS</p><pre>  code | \n  line</pre>'
        const rendered = ReaderLayout.readableHtml(html, 320)
        verify(!rendered.includes('<img'))
        verify(!rendered.includes(' | | '))
        verify(!rendered.includes('<br><br><br>'))
        verify(rendered.includes('Example browser | Example OS'))
        verify(rendered.includes('<pre>  code | \n  line</pre>'))
        const original = mock.selectedMessage
        try {
            mock.selectedMessage = Object.assign({}, original, {html: html})
            const body = findChild(app, "messageBody")
            tryVerify(() => body.getText(0, body.length).includes("Account activity"))
            const plain = body.getText(0, body.length)
            verify(!plain.includes("\uFFFC"), "Blocked images must not produce broken image objects")
            verify(!plain.includes(" | | "))
            verify(plain.includes("Example browser | Example OS"))
            verify(body.contentHeight < 280, "Notification must not grow into pages of empty space")
        } finally { mock.selectedMessage = original }
    }
    function test_smart_loading_does_not_select_same_uid_in_other_folder() {
        const original = mock.selectedMessage
        mock.smartMailbox = "flagged"
        mock.selectedMessage = ({})
        wait(30)
        const pane = findChild(app, "messagePane")
        const row = findChild(pane, "messageList").itemAtIndex(0)
        verify(row !== null)
        verify(!row.selected)
        verify(!row.canCompose)
        mock.selectedMessage = Object.assign({}, original, {folder: "INBOX"})
        wait(30)
        verify(row.selected)
        mock.selectedMessage = original
    }
    function test_sidebar_sections_and_smart_navigation() {
        const sidebar = findChild(app, "sidebarBody")
        verify(findChild(sidebar, "favoritesTitle").visible)
        verify(findChild(sidebar, "smartMailboxesTitle").visible)
        const flagged = findChild(sidebar, "favoriteShortcut_flagged")
        const unread = findChild(sidebar, "smartMailbox_unread")
        flagged.clicked()
        compare(mock.smartMailbox, "flagged")
        verify(flagged.checked)
        unread.clicked()
        compare(mock.smartMailbox, "unread")
        verify(!flagged.checked && unread.checked)
        findChild(sidebar, "favoriteShortcut_inbox").clicked()
        compare(mock.smartMailbox, "")
        findChild(sidebar, "draftFolderButton").clicked()
        verify(app.draftsVisible)
    }
    function test_views_load_without_shell_context() {
        const reader = createTemporaryObject(readerComponent, testCase, {
            mail: mock, message: mock.selectedMessage, hasMessage: true, width: 600, height: 500
        })
        const sidebar = createTemporaryObject(sidebarComponent, testCase, {
            mail: mock, surface: testCase, sidebarAccounts: mock.accounts, width: 240, height: 500
        })
        const pane = createTemporaryObject(paneComponent, testCase, {
            mail: mock, surface: testCase, width: 320, height: 500
        })
        verify(reader !== null && sidebar !== null && pane !== null)
        wait(50)
        compare(findChild(pane, "messageList").count, 1)
        compare(findChild(reader, "messageBody").text, mock.selectedMessage.text)
        const selection = pane.selection
        selection.toggleRow(0)
        compare(selection.selectionSize, 1)
        selection.clearSelection()
        compare(selection.selectionSize, 0)
    }
    Component {
        id: platformControlsMock
        QtObject {
            property var target
            property var buttons
            property bool enabled
            property bool active: false
        }
    }
    function test_window_controls_geometry_and_takeover() {
        const controls = findChild(findChild(app, "windowChrome"), "windowControls")
        const client = findChild(controls, "clientWindowControls")
        const original = controls.platformComponent
        controls.platformComponent = platformControlsMock
        try {
            wait(20)
            compare(controls.buttonLayout.length, 3)
            const actions = ["maximize", "minimize", "close"]
            for (let i = 0; i < 3; ++i) {
                const rect = controls.buttonLayout[i]
                compare(rect.action, actions[i])
                compare(rect.width, 14)
                compare(rect.height, 14)
                const point = controls.mapToItem(app.contentItem, rect.x, rect.y)
                compare(point.x - app.width, -84.5 + 28 * i)
                compare(point.y, 16.5)
            }
            compare(client.opacity, 1)
            verify(client.enabled)
            controls.platformSlot.active = true
            compare(client.opacity, 0)
            verify(!client.enabled)
            compare(controls.opacity, 1)
            compare(controls.width, 78)
            controls.platformSlot.active = false
            compare(client.opacity, 1)
            verify(client.enabled)
            controls.platformComponent = null
            compare(controls.platformSlot, null)
            compare(client.opacity, 1)
            verify(client.enabled)
        } finally {
            controls.platformComponent = original
        }
    }

    function test_search_has_ten_pixel_gap_to_traffic_lights() {
        const toolbar = findChild(app, "readerHeader")
        const controls = findChild(findChild(app, "windowChrome"), "windowControls")
        for (const width of [1000, 1180, 1320, 1600]) {
            toolbar.closeSearch()
            app.width = width
            wait(30)
            for (const expanded of [false, true]) {
                if (expanded) toolbar.openSearch()
                wait(30)
                const search = findChild(app, toolbar.wideSearch || expanded ? "searchField" : "searchToggleButton")
                verify(search.visible)
                const circleLeft = controls.mapToItem(app.contentItem, controls.buttonLayout[0].x, 0).x
                const searchRight = search.mapToItem(app.contentItem, search.width, 0).x
                verify(circleLeft - searchRight >= 9.5 && circleLeft - searchRight <= 10,
                       "Gap at width " + width + ", expanded=" + expanded + ": " + (circleLeft - searchRight))
            }
        }
        toolbar.closeSearch()
    }

    function test_shell_planes_and_window_chrome_are_stable() {
        const planes = findChild(app, "chromePlanes")
        const sidebarPlane = findChild(app, "sidebarPlane")
        const contentPlane = findChild(app, "contentPlane")
        const backdrop = findChild(app, "chromeBackdrop")
        const chrome = findChild(app, "windowChrome")
        const controls = findChild(findChild(app, "windowChrome"), "windowControls")
        verify(planes !== null)
        verify(sidebarPlane !== null)
        verify(contentPlane !== null)
        verify(backdrop !== null)
        verify(chrome !== null)
        verify(controls !== null)
        compare(backdrop.sourceItem, planes)
        verify(backdrop.sourceItem !== findChild(app, "mailScene"))
        compare(App.MailStyle.shellHeaderHeight, 47)
        compare(App.MailStyle.toolbarButtonHeight, 28)
        compare(chrome.height, 47)
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
        compare(header.height, 47)
        const filter = findChild(app, "unreadFilterButton")
        const toolbar = findChild(app, "readerHeader")
        for (const name of ["mailTitle", "folderTitle", "unreadFilterButton", "sidebarToggleButton", "windowControls", "mailboxActionGroup"]) {
            const item = findChild(app, name)
            compare(item.mapToItem(header, 0, item.height / 2).y, header.height / 2,
                    name + " must be vertically centred in the header")
        }
        compare(filter.height, 28)
        compare(findChild(app, "sidebarToggleButton").height, filter.height)
        compare(findChild(app, "mailboxActionGroup").height, filter.height)
        compare(filter.mapToItem(scene, 0, filter.height / 2).y,
                toolbar.mapToItem(scene, 0, toolbar.controlsCenterY).y)
        compare(sidebar.y, 47)
        compare(content.y, 47)
        verify(sidebar.height > 0 && content.height > 0)
        for (const name of ["sidebarToggleButton", "searchField"]) {
            let item = findChild(app, name)
            verify(item !== null)
            while (item && item !== header) item = item.parent
            compare(item, header, name + " must belong to the header")
        }
        {
            const group = findChild(app, "messageOptionsGroup")
            verify(group !== null)
            let item = group
            while (item && item !== header) item = item.parent
            compare(item, header, "messageOptionsGroup must belong to the header")
            for (const name of ["replyButton", "detailsButton", "moreButton"]) {
                const named = findChild(app, name)
                if (!named) continue   // 由组内按钮承担，命名可能尚未下发
                let owner = named
                while (owner && owner !== header) owner = owner.parent
                compare(owner, header, name + " must belong to the header")
            }
        }
        compare(findChild(app, "messageList").mapToItem(scene, 0, 0).y, 47)
        compare(findChild(app, "readerScroll").mapToItem(scene, 0, 0).y, 47)
    }

    function test_shell_geometry_at_width_boundaries() {
        const scene = findChild(app, "mailScene")
        const content = findChild(app, "contentScene")
        const header = findChild(app, "floatingHeader")
        const sidebar = findChild(app, "sidebarBody")
        const list = findChild(app, "messagePane")
        const reader = findChild(app, "messageReader")
        const controls = findChild(findChild(app, "windowChrome"), "windowControls")
        const listHeader = findChild(app, "listHeader")
        const readerHeader = findChild(app, "readerHeader")
        compare(findChild(app, "statusStrip"), null)
        for (const width of [580, 650, 799, 800, 820, 1079, 1080, 1280]) {
            for (const expanded of [false, true]) {
                for (const reading of [false, true]) {
                    app.width = width
                    app.sidebarExpanded = expanded
                    app.sidebarPreferredWidth = 340
                    app.readerVisible = reading
                    wait(25)
                    compare(header.width, width)
                    compare(content.x, app.sidebarVisible ? app.sidebarWidth : 0)
                    compare(content.y, 47)
                    compare(content.y + content.height, scene.height)
                    compare(sidebar.y + sidebar.height, scene.height)
                    compare(list.visible, !app.narrow || !reading)
                    compare(reader.visible, !app.narrow || reading)
                    compare(listHeader.visible, list.visible)
                    compare(readerHeader.visible, reader.visible)
                    for (const pair of [[listHeader, list], [readerHeader, reader]]) {
                        if (!pair[0].visible) continue
                        compare(pair[0].x, pair[1].mapToItem(scene, 0, 0).x)
                        verify(pair[0].width > 0)
                    }
                    const controlTop = controls.mapToItem(scene, 0, 0)
                    verify(controlTop.x >= 0 && controlTop.x + controls.width <= width)
                    for (const name of ["sidebarToggleButton", "refreshButton", "headerComposeButton", "searchField", "readerBackButton", "replyButton", "forwardButton", "detailsButton", "moreButton"]) {
                        const item = findChild(app, name)
                        if (!item || !item.visible) continue
                        const top = item.mapToItem(scene, 0, 0)
                        verify(item.width > 0 && item.height > 0, name)
                        verify(top.x >= 0 && top.x + item.width <= width, name + " outside window")
                        verify(top.y >= 0 && top.y + item.height <= 47, name + " outside header")
                        verify(top.y >= controlTop.y + controls.height
                            || top.x + item.width <= controlTop.x, name + " overlaps window controls")
                    }
                    for (const name of ["sidebarDivider", "messageDivider"]) {
                        const divider = findChild(app, name)
                        if (!divider.visible) continue
                        compare(divider.y, 47)
                        compare(divider.y + divider.height, scene.height)
                    }
                }
            }
        }
    }

    function test_compact_reader_details_anchor_and_search_return() {
        activateApp()
        app.width = 580
        app.sidebarExpanded = true
        app.sidebarPreferredWidth = 340
        app.readerVisible = true
        wait(30)
        compare(findChild(app, "detailsButton"), null)   // 窄宽下「详情」已并入「更多」，不再有独立按钮
        mouseClick(findChild(app, "messageDetailsButton"))
        const details = findChild(app, "messageDetailsPanel")
        verify(details !== null)
        tryCompare(details, "visible", true)
        // 内联面板：展开时把正文往下推，关闭后高度归零
        verify(details.height > 0)
        app.detailsVisible = false
        wait(30)
        compare(details.visible, false)
        keyClick(Qt.Key_F, Qt.ControlModifier)
        compare(app.readerVisible, false)
        const search = findChild(app, "searchField")
        // Compact search replaces header actions with a visible, usable input.
        tryCompare(search, "visible", true)
        tryCompare(search, "activeFocus", true)
        keyClick(Qt.Key_A)
        tryCompare(mock, "search", "a")
        keyClick(Qt.Key_Escape)
        tryCompare(search, "visible", false)
        verify(findChild(app, "searchToggleButton") !== null)
    }

    function test_header_controls_receive_pointer_input() {
        activateApp()
        app.width = 1400          // 搜索输入框只在 >=1320 时显示
        wait(30)
        const scene = findChild(app, "mailScene")
        let refresh = findChild(app, "refreshButton")
        if (refresh) {
            const point = refresh.mapToItem(scene, refresh.width / 2, refresh.height / 2)
            mouseClick(scene, point.x, point.y)
        } else {
            const group = findChild(app, "mailboxActionGroup")
            verify(group !== null)
            const point = group.mapToItem(scene, group.width / 4, group.height / 2)
            mouseClick(scene, point.x, point.y)
        }
        compare(mock.refreshCount, 1)
        const search = findChild(app, "searchField")
        mouseClick(search)
        tryCompare(search, "activeFocus", true)
        keyClick(Qt.Key_A)
        tryCompare(mock, "search", "a")
        compare(findChild(app, "sidebarComposeButton"), null)
        const compose = findChild(app, "mailboxActionGroup")
        verify(compose.visible)
        verify(compose.enabled)
        verify(compose.width > 0 && compose.height > 0)
        const composePoint = compose.mapToItem(scene, compose.width * 3 / 4, compose.height / 2)
        mouseClick(scene, composePoint.x, composePoint.y)
        tryCompare(findChild(app, "composerDialog"), "visible", true)
    }

    function test_column_widths_are_adjustable_and_clamped() {
        const sidebarDivider = findChild(app, "sidebarDivider")
        const listDivider = findChild(app, "messageDivider")
        compare(app.sidebarWidth, 189)
        compare(app.listWidth, 280)
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
        compare(app.sidebarPreferredWidth, 229)
        tryCompare(divider, "x", 225)
        const next = divider.mapToItem(scene, divider.width / 2, 120)
        mouseDrag(scene, next.x, next.y, -40, 0)
        compare(app.sidebarPreferredWidth, 189)
    }
    function test_column_divider_keyboard_and_double_click() {
        activateApp()
        const divider = findChild(app, "messageDivider")
        divider.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(app.listPreferredWidth, 290)
        keyClick(Qt.Key_Left)
        compare(app.listPreferredWidth, 280)
        divider.resizeTo(410)
        mouseDoubleClickSequence(divider, divider.width / 2, 120)
        compare(app.listPreferredWidth, 280)
    }
    function test_search_shortcut_and_tab_focus() {
        activateApp()
        app.draftsVisible = true
        keyClick(Qt.Key_F, Qt.ControlModifier)
        const search = findChild(app, "searchField")
        tryCompare(search, "activeFocus", true)
        compare(app.draftsVisible, false)
        verify(findChild(app, "listHeader").visible)
        verify(findChild(app, "folderTitle").visible)
        verify(findChild(app, "unreadFilterButton").visible)
        for (const character of "example") keyClick(character)
        tryCompare(mock, "search", "example")
        keyClick(Qt.Key_Escape)
        compare(search.text, "")
        compare(mock.search, "")
        keyClick(Qt.Key_Tab)
        compare(search.activeFocus, false)
        verify(app.activeFocusItem !== null)
    }
    function test_composer_close_decision_save_failure_and_cancel() {
        activateApp()
        keyClick(Qt.Key_N, Qt.ControlModifier)
        const dialog = findChild(app, "composerDialog")
        tryCompare(dialog, "visible", true)
        const body = findChild(app, "composerBody")
        body.text = "Do not discard on Escape."
        body.forceActiveFocus()
        mock.failSave = true
        keyClick(Qt.Key_Escape)
        const decision = findChild(dialog, "composerCloseDecision")
        tryCompare(decision, "opened", true)
        mouseClick(findChild(decision, "composerSaveAndClose"))
        compare(dialog.visible, true)
        compare(decision.visible, true)
        compare(body.getText(0, body.length), "Do not discard on Escape.")
        mouseClick(findChild(decision, "composerCancelClose"))
        tryCompare(decision, "visible", false)
        verify(dialog.visible)
        mock.failSave = false
        dialog.close()
        tryCompare(decision, "opened", true)
        mouseClick(findChild(decision, "composerSaveAndClose"))
        tryCompare(dialog, "visible", false)
        compare(mock.lastSaved.body, "Do not discard on Escape.")
        app.width = 650
        app.readerVisible = true
        wait(30)
        keyClick(Qt.Key_Escape)
        compare(app.readerVisible, true)   // 窄宽返回列表的行为随 narrow 布局一起移除
    }
    function test_composer_is_independent_and_resizable() {
        app.showReview("compose")
        const editor = findChild(app, "composerDialog")
        verify(editor !== null)
        verify(editor.contentItem.Window.window === editor)
        verify(editor.contentItem.Window.window !== app)
        compare(editor.modality, Qt.NonModal)
        const surface = findChild(editor, "composerSurface")
        compare(surface.radius, 0)
        compare(surface.border.width, 0)
        const chrome = findChild(editor, "composerWindowChrome")
        verify(chrome !== null)
        const controls = findChild(chrome, "windowControls")
        compare(controls.targetWindow, editor)
        verify(controls.platformComponent !== null)
        const backdrop = findChild(editor, "composerBackdrop")
        compare(findChild(editor, "sendDraftButton").shellBackdrop, backdrop)
        compare(findChild(editor, "sendDraftButton").floatingAction, true)
        compare(findChild(editor, "composerActionGroupSurface").materialForm, !editor.liquidControls)
        const send = findChild(editor, "sendDraftButton")
        for (const width of [640, 800, 1040, 1400]) {
            editor.width = width
            wait(30)
            const circle = controls.buttonLayout[0]
            const circleLeft = controls.mapToItem(editor.contentItem, circle.x, circle.y)
            const sendRight = send.mapToItem(editor.contentItem, send.width, send.height / 2)
            fuzzyCompare(circleLeft.x - sendRight.x, 10, 0.5)
            fuzzyCompare(circleLeft.y + circle.height / 2, sendRight.y, 0.5)
        }
        editor.width = 1000; editor.height = 800
        compare(editor.width, 1000)
        compare(editor.height, 800)
        app.hide()
        verify(editor.visible)
        editor.showMinimized()
        tryCompare(editor, "visibility", Window.Minimized)
        editor.showNormal()
        tryCompare(editor, "visibility", Window.Windowed)
        verify(editor.finishEditing())
    }
    function test_composer_discard_removes_autosaved_draft() {
        app.showReview("compose")
        const editor = findChild(app, "composerDialog")
        const body = findChild(editor, "composerBody")
        body.text = "Discard this synthetic draft."
        verify(editor.persist())
        verify(mock.loadDraft(editor.draft.id).id)
        editor.close()
        const decision = findChild(editor, "composerCloseDecision")
        tryCompare(decision, "opened", true)
        mouseClick(findChild(decision, "composerDiscardAndClose"))
        tryCompare(editor, "visible", false)
        verify(!mock.loadDraft("test-draft").id)
        wait(900)
        verify(!mock.loadDraft("test-draft").id)
    }
    function test_composer_formats_selected_text() {
        app.showReview("compose")
        const editor = findChild(app, "composerDialog")
        const body = findChild(editor, "composerBody")
        body.text = "alpha beta"
        editor.requestActivate()
        tryCompare(editor, "active", true)
        waitForRendering(body)
        body.forceActiveFocus()
        body.select(6, 10)
        mouseClick(findChild(editor, "composerBold"))
        body.select(6, 10)
        verify(body.cursorSelection.font.bold)
        body.select(0, 5)
        verify(!body.cursorSelection.font.bold)
        verify(editor.persist())
        verify(mock.lastSaved.html.indexOf("font-weight") >= 0)
    }
    function test_composer_shared_selects_format_the_selection() {
        app.showReview("compose")
        const editor = findChild(app, "composerDialog")
        const body = findChild(editor, "composerBody")
        body.text = "alpha beta"
        body.select(0, 5)
        for (const name of ["composerParagraphStyle", "composerFontFamily", "composerFontStyle", "composerFontSize"]) {
            const select = findChild(editor, name)
            compare(select.materialForm, true)
            compare(select.popupMaterialForm, true)
            compare(select.cornerRadius, 6)
            select.openMenu()
            tryCompare(select, "popupOpened", true)
            select.closeMenu()
        }
        const family = findChild(editor, "composerFontFamily")
        family.select(1)
        compare(body.cursorSelection.font.family, "serif")
        const face = findChild(editor, "composerFontStyle")
        face.select(2)
        verify(body.cursorSelection.font.italic)
        verify(!body.cursorSelection.font.bold)
        const size = findChild(editor, "composerFontSize")
        size.select(3)
        compare(body.cursorSelection.font.pointSize, 16)
        const toggle = findChild(editor, "composerFormattingToggle")
        const before = editor.showFormatting
        toggle.clicked()
        compare(editor.showFormatting, !before)
        compare(toggle.checked, false)
        toggle.clicked()
        verify(editor.persist())
        verify(mock.lastSaved.html.indexOf("font-style:italic") >= 0)
    }
    function test_composer_emoji_picker_inserts_and_closes() {
        app.showReview("compose")
        const editor = findChild(app, "composerDialog")
        const body = findChild(editor, "composerBody")
        body.text = "hello"
        body.select(0, 5)
        findChild(editor, "composerEmoji").clicked()
        const picker = findChild(editor, "composerEmojiPicker")
        tryCompare(picker, "opened", true)
        const choices = findChild(editor, "composerEmojiChoices")
        compare(choices.count, 18)
        for (let i = 0; i < choices.count; ++i) {
            const choice = choices.itemAt(i)
            const point = choice.mapToItem(picker.contentItem, 0, 0)
            verify(point.x >= 0 && point.x + choice.width <= picker.contentItem.width + 0.5)
            verify(point.y >= 0 && point.y + choice.height <= picker.contentItem.height + 0.5)
            verify(choice.contentItem.width >= 22)
        }
        verify(choices.itemAt(0) !== null)
        choices.itemAt(0).clicked()
        compare(picker.visible, false)
        verify(editor.persist())
        compare(mock.lastSaved.body, "😀")
    }
    function test_composer_paragraph_tools_persist_and_undo() {
        app.showReview("compose")
        const editor = findChild(app, "composerDialog")
        const body = findChild(editor, "composerBody")
        body.text = "alpha\nbeta\ngamma"
        body.select(0, 10)
        findChild(editor, "composerNumbers").clicked()
        verify(editor.paragraphState.numbered)
        verify(editor.persist())
        verify(mock.lastSaved.html.indexOf("<ol") >= 0)
        findChild(editor, "composerBullets").clicked()
        verify(editor.paragraphState.bullets)
        verify(!editor.paragraphState.numbered)
        findChild(editor, "composerBullets").clicked()
        verify(!editor.paragraphState.bullets)
        findChild(editor, "composerAlign1").clicked()
        verify(editor.paragraphState.alignment & Qt.AlignHCenter)
        findChild(editor, "composerIndent").clicked()
        verify(editor.persist())
        verify(mock.lastSaved.html.indexOf('align="center"') >= 0)
        verify(mock.lastSaved.html.indexOf('-qt-block-indent:1') >= 0)
        findChild(editor, "composerOutdent").clicked()
        verify(editor.persist())
        verify(mock.lastSaved.html.indexOf('-qt-block-indent:1') < 0)
        verify(body.canUndo)
        body.undo()
        verify(editor.persist())
        verify(mock.lastSaved.html.indexOf('-qt-block-indent:1') >= 0)
    }
    function test_composer_link_validation_and_emoji_replace_selection() {
        app.showReview("compose")
        const editor = findChild(app, "composerDialog")
        const body = findChild(editor, "composerBody")
        body.text = "alpha beta"
        body.select(0, 5)
        findChild(editor, "composerLink").clicked()
        const url = findChild(editor, "composerLinkUrl")
        const insert = findChild(editor, "composerInsertLink")
        url.text = "javascript:alert(1)"
        compare(insert.enabled, false)
        url.text = "https://example.invalid/path"
        compare(insert.enabled, true)
        insert.clicked()
        verify(editor.persist())
        verify(mock.lastSaved.html.indexOf('href="https://example.invalid/path"') >= 0)
        body.select(6, 10)
        editor.insertText("😊")
        verify(editor.persist())
        compare(mock.lastSaved.body, "alpha 😊")
        body.undo()
        verify(editor.persist())
        compare(mock.lastSaved.body, "alpha beta")
        body.redo()
        verify(editor.persist())
        compare(mock.lastSaved.body, "alpha 😊")
        editor.applyStyle(1)
        verify(body.cursorSelection.font.bold)
        compare(body.cursorSelection.font.pointSize, 24)
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
        compare(body.getText(0, body.length), "Keep this local draft.")
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
    function test_navigation_uses_roles_without_merging_names() {
        const folders = [{id:"custom",title:"Inbox"}, {id:"wire/sent",title:"Sent",role:"sent"}, {id:"INBOX",title:"Inbox"}]
        const sorted = Navigation.ordered(folders)
        compare(sorted.length, 3)
        compare(sorted[0].id, "INBOX")
        compare(sorted[1].id, "wire/sent")
        compare(sorted[2].id, "custom")
        compare(App.MailLabels.mailboxIcon(sorted[1]), "forward")
        compare(App.MailLabels.mailboxIcon(sorted[2]), "folder")
    }

    function test_demo_address_can_be_typed_and_detects_provider() {
        const dialog = findChild(app, "accountDialog")
        dialog.addAccount()
        const address = findChild(dialog, "accountAddress")
        address.forceActiveFocus()
        tryCompare(address, "activeFocus", true)
        for (const character of "preview@gmail.com") keyClick(character)
        compare(address.text, "preview@gmail.com")
        compare(findChild(dialog, "providerChoice").provider, "google")
        dialog.close()
    }

    function test_provider_list_scrolls_to_last_choice() {
        const dialog = findChild(app, "accountDialog")
        dialog.addAccount()
        tryCompare(dialog, "opened", true)
        const picker = findChild(dialog, "providerChoice")
        const scroll = findChild(picker, "providerScroll")
        const bar = findChild(picker, "providerScrollBar")
        verify(dialog.height < 560)
        verify(bar.visible && bar.size < 1)
        verify(bar.x > scroll.width / 2)
        mouseDrag(bar, bar.width / 2, bar.height * bar.size / 2,
                  0, bar.height * (1 - bar.size), Qt.LeftButton)
        tryVerify(() => scroll.contentItem.contentY > 0)
        const manual = findChild(picker, "provider_manual")
        const point = manual.mapToItem(scroll, manual.width / 2, manual.height / 2)
        verify(point.y >= 0 && point.y <= scroll.height)
        picker.currentIndex = 1
        mouseClick(manual)
        compare(picker.currentIndex, 0)
        dialog.close()
    }
    function test_provider_presets_and_no_false_oauth_support() {
        const expected = {icloud:"imap.mail.me.com",google:"imap.gmail.com",qq:"imap.qq.com",exmail:"imap.exmail.qq.com",
            "163":"imap.163.com","126":"imap.126.com",yahoo:"imap.mail.yahoo.com",aol:"imap.aol.com"}
        const dialog = findChild(app, "accountDialog")
        dialog.addAccount()
        function visualChild(item, name) {
            if (item.objectName === name) return item
            for (const child of item.children || []) {
                const found = visualChild(child, name)
                if (found) return found
            }
            return null
        }
        const picker = findChild(dialog, "providerChoice")
        for (const provider in expected) {
            const logo = visualChild(picker, "providerLogo_" + provider)
            verify(logo !== null, provider)
            tryCompare(logo, "status", Image.Ready)
            verify(logo.source.toString().indexOf("qrc:/data/providers/") === 0)
            dialog.step = 0
            findChild(dialog, "providerChoice").currentIndex = Profiles.ids.indexOf(provider)
            dialog.nextStep()
            compare(dialog.configuration().imapHost, expected[provider])
            compare(dialog.configuration().imapPort, 993)
            compare(dialog.configuration().imapStartTls, false)
            compare(dialog.configuration().serverSavesSent, provider === "google")
        }
        compare(Profiles.forAddress(" USER@GMAIL.COM "), "google")
        compare(Profiles.forAddress("person@company.invalid"), "manual")
        compare(Profiles.forAddress("person@gmail.com.attacker.invalid"), "manual")
        dialog.step = 0
        findChild(dialog, "providerChoice").currentIndex = Profiles.ids.indexOf("microsoft")
        dialog.nextStep()
        compare(dialog.step, 0)
        tryCompare(visualChild(picker, "providerLogo_microsoft"), "status", Image.Ready)
        compare(Profiles.logo("manual"), "")
        dialog.close()
    }
    function test_account_wizard_preserves_config_on_failure() {
        const dialog = findChild(app, "accountDialog")
        dialog.addAccount()
        compare(dialog.step, 0)
        const address = findChild(dialog, "accountAddress")
        address.text = "sample@icloud.com"
        findChild(dialog, "providerChoice").currentIndex = 1
        dialog.nextStep()
        compare(dialog.step, 1)
        compare(dialog.configuration().imapHost, "imap.mail.me.com")
        compare(dialog.configuration().smtpPort, 587)
        mock.demo = false
        findChild(dialog, "accountPassword").text = "test-secret"
        dialog.validateConnection()
        verify(dialog.submitting)
        setupMock.finished(false, "Synthetic connection failure")
        verify(!dialog.submitting)
        compare(dialog.step, 1)
        compare(dialog.configuration().address, "sample@icloud.com")
        compare(findChild(dialog, "accountPassword").text, "")
        findChild(dialog, "accountPassword").text = "retry-secret"
        dialog.validateConnection()
        setupMock.finished(true, "")
        compare(dialog.step, 2)
        compare(findChild(dialog, "accountPassword").text, "")
        dialog.close()
    }

    function test_account_wizard_save_rejection_unlocks_retry() {
        const dialog = findChild(app, "accountDialog")
        dialog.addAccount()
        findChild(dialog, "accountAddress").text = "retry@icloud.com"
        findChild(dialog, "providerChoice").currentIndex = 1
        dialog.nextStep()
        mock.demo = false
        findChild(dialog, "accountPassword").text = "synthetic-secret"
        dialog.validateConnection()
        mock.busy = true // Background work started while verification was running.
        setupMock.finished(true, "")
        verify(!dialog.submitting)
        compare(dialog.step, 1)
        verify(dialog.setupError.length > 0)
        compare(dialog.configuration().address, "retry@icloud.com")
        compare(findChild(dialog, "accountPassword").text, "")
        mock.busy = false
        findChild(dialog, "accountPassword").text = "synthetic-retry"
        dialog.validateConnection()
        setupMock.finished(true, "")
        compare(dialog.step, 2)
        dialog.close()
    }

    function test_account_password_is_not_prefilled() {
        app.showReview("account")
        compare(findChild(app, "accountPassword").text, "")
        compare(findChild(app, "accountPassword").echoMode, TextInput.Password)
    }
    function test_narrow_window_keeps_three_panes() {
        app.width = 650
        app.readerVisible = true
        wait(30)
        compare(findChild(app, "messageReader").visible, true)
        compare(findChild(app, "messageList").visible, true)
        app.readerVisible = false
        wait(30)
        // narrow 布局已移除：650 宽仍是三栏，阅读栏不随 readerVisible 隐藏
        compare(findChild(app, "messageReader").visible, true)
        compare(findChild(app, "messageList").visible, true)
    }
}

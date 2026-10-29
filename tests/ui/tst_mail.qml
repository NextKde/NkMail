import QtQuick
import QtTest
import Kos.Ui
import "../../qml" as App

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
        property bool failSave: false
        property int saveCount: 0
        property var lastSaved: ({})
        signal selectionChanged()
        signal draftAccepted(string id)
        ListModel {
            id: messageModel
            ListElement { uid: "1"; uidValidity: "1"; folder: "INBOX"; sender: "Studio Example"; senderAddress: "studio@example.invalid"; subject: "A quieter inbox"; date: "2026-10-29T09:00:00Z"; preview: "A synthetic message."; unread: true; flagged: false; hasAttachments: false }
        }
        function configureAccount(config, password, remember) {}
        function refresh() {}
        function selectFolder(folder) { currentFolder = folder }
        function loadMore() {}
        function openMessage(uid, validity) { selectedUid = uid }
        function newDraft(mode) { return {id: "test-draft", to: "", cc: "", bcc: "", subject: "", body: "", inReplyTo: "", references: "", state: "draft", attachments: []} }
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
        AppTheme.appearanceMode = "light"
        mock.failSave = false; mock.saveCount = 0; mock.lastSaved = ({})
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

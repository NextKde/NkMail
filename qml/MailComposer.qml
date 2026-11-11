import QtQuick
import Kos.Icons as Symbols
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import Kos.Ui
import NextKde.Mail.Editor 1.0

ApplicationWindow {
    id: root
    required property var mail
    required property var hostWindow
    property var draft: ({})
    property bool populating: false
    property bool dirty: false
    property bool saveFailed: false
    property string localStatus: ""
    property bool showBcc: false
    property bool showFormatting: true
    property string fromAddress: ""
    property var paragraphState: ({ alignment: Qt.AlignLeft, bullets: false, numbered: false })
    readonly property var availableFonts: ["sans-serif", "serif", "monospace"].concat(Qt.fontFamilies().filter(family => ["sans-serif", "serif", "monospace"].indexOf(family) < 0))
    readonly property bool accountMatches: !draft.accountId || draft.accountId === mail.currentAccountId
    readonly property bool editable: accountMatches && !mail.sending && !mail.sendPending
    objectName: "composerDialog" // Preserve the public review/test lookup.
    width: 1040; height: 740
    minimumWidth: 640; minimumHeight: 500
    visible: false
    modality: Qt.NonModal
    // A separate top-level window remains usable while the inbox is minimized.
    transientParent: null
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
    color: "transparent"
    title: subjectField.text.trim() || qsTr("New message")
    background: Item {}
    readonly property bool liquidControls: AppTheme.materialMode !== "solid"
        && !AppTheme.reduceTransparency && GraphicsInfo.api !== GraphicsInfo.Software
    font.family: "sans-serif"
    font.pixelSize: 13

    component ComposerAction: MailAction {
        toolbarAction: false
        implicitHeight: 28
        implicitWidth: iconOnly ? 32 : Math.max(44, contentItem.implicitWidth + leftPadding + rightPadding)
        accessibleText: text
        focusPolicy: Qt.TabFocus
    }
    // Keep the inline component as a host Item: directly deriving it from the
    // imported QML icon type crashes Qt 6.10's GC during repeated dialog use.
    component ComposerIcon: Item {
        id: composerGlyph
        required property string glyph
        property color ink: AppTheme.text
        implicitWidth: 16
        implicitHeight: 16
        Symbols.Icon {
            anchors.centerIn: parent
            size: 16
            name: composerGlyph.glyph === "send" ? "arrow_up" : composerGlyph.glyph
            color: composerGlyph.ink
        }
    }
    component HeaderAction: ComposerAction {
        id: headerAction
        required property string glyph
        iconOnly: true
        implicitWidth: 32; implicitHeight: 28
        contentItem: Item {
            ComposerIcon {
                anchors.centerIn: parent
                width: 16; height: 16
                glyph: headerAction.glyph
                ink: headerAction.textColor
            }
        }
    }
    component FormatAction: ComposerAction {
        implicitWidth: 26
        implicitHeight: 24
        materialForm: true
        flat: true
        iconOnly: true
        leftPadding: 0; rightPadding: 0
        cornerRadius: 6
    }
    component FormatGroup: Item {
        default property alias actions: groupActions.data
        implicitWidth: groupActions.implicitWidth
        implicitHeight: 24
        LiquidActionButton {
            anchors.fill: parent
            materialForm: true
            cornerRadius: 6
            enabled: false
            Accessible.ignored: true
        }
        Row { id: groupActions; spacing: 0 }
    }
    component FormatSelect: LiquidSelect {
        implicitHeight: 24
        cornerRadius: 6
        materialForm: true
        popupMaterialForm: true
        compactPopup: true
        neutralOptionHighlight: true
        focusRingColor: "transparent"
        popupFontWeight: Font.Normal
        appearanceMode: AppTheme.dark ? "dark" : "light"
    }
    component ListAction: FormatAction {
        id: listAction
        property bool numbered: false
        contentItem: Symbols.Icon { name: listAction.numbered ? "list_number" : "list_bullet"; size: 16; color: AppTheme.text }
    }
    component AlignmentAction: FormatAction {
        id: alignmentAction
        required property int paragraphAlignment
        checkable: true
        checked: !!(root.paragraphState.alignment & paragraphAlignment)
        contentItem: Symbols.Icon { name: alignmentAction.paragraphAlignment === Qt.AlignHCenter ? "text_aligncenter" : alignmentAction.paragraphAlignment === Qt.AlignRight ? "text_alignright" : "text_alignleft"; size: 16; color: AppTheme.text }
        onClicked: root.format(() => richEditor.setAlignment(bodyField.textDocument, paragraphAlignment))
    }
    component AddressField: TextField {
        Layout.fillWidth: true
        implicitHeight: 42
        color: AppTheme.text
        selectionColor: AppTheme.accent
        selectedTextColor: AppTheme.accentText
        font.pixelSize: 14
        selectByMouse: true
        leftPadding: 0; rightPadding: 0
        background: Item {}
        enabled: root.editable
        onTextChanged: root.changed()
    }
    component FieldLabel: Text {
        Layout.preferredWidth: 58
        color: AppTheme.mutedText
        font.pixelSize: 13
    }
    component Rule: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: AppTheme.border
    }

    function toggleMaximized() {
        if (visibility === Window.Maximized) showNormal()
        else showMaximized()
    }
    function copy(map) { return JSON.parse(JSON.stringify(map || {})) }
    function editDraft(value) {
        if (visible && !persist()) return false
        if (!value || !value.id) return false
        populating = true
        draft = copy(value)
        fromAddress = String(root.mail.account.address || "")
        toField.text = String(draft.to || "")
        ccField.text = String(draft.cc || "")
        bccField.text = String(draft.bcc || "")
        subjectField.text = String(draft.subject || "")
        const plainBody = String(draft.body || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        bodyField.text = draft.html ? String(draft.html) : "<p>" + plainBody.replace(/\n/g, "<br>") + "</p>"
        showBcc = bccField.text.length > 0
        dirty = false; saveFailed = false
        localStatus = qsTr("Saved locally")
        populating = false
        closeDecision.close()
        showNormal()
        requestActivate()
        toField.forceActiveFocus()
        return true
    }
    function changed() {
        if (populating || !visible) return
        dirty = true; saveFailed = false
        localStatus = qsTr("Saving…")
        autosave.restart()
    }
    function currentDraft() {
        const value = copy(draft)
        value.to = toField.text; value.cc = ccField.text; value.bcc = bccField.text
        value.subject = subjectField.text
        value.body = richEditor.toPlainText(bodyField.textDocument)
        value.html = richEditor.toHtml(bodyField.textDocument)
        return value
    }
    function persist() {
        autosave.stop()
        if (!draft.id) return false
        if (!dirty && !saveFailed) return true
        const saved = mail.saveDraft(currentDraft())
        if (!saved || !saved.id) {
            saveFailed = true
            localStatus = qsTr("Could not save. Keep this window open and try again.")
            return false
        }
        draft = copy(saved)
        dirty = false; saveFailed = false
        localStatus = qsTr("Saved locally")
        return true
    }
    // Explicit save-and-close is separate from the window's close request.
    function finishEditing() {
        if (mail.sending || mail.sendPending || !persist()) return false
        closeDecision.close()
        hide()
        return true
    }
    function requestClose() {
        if (mail.sending || mail.sendPending) {
            requestActivate()
            return false
        }
        autosave.stop()
        closeDecision.open()
        return false
    }
    function discardAndClose() {
        if (!editable || !draft.id) return false
        autosave.stop()
        mail.discardDraft(draft.id)
        const remaining = mail.loadDraft(draft.id)
        if (remaining && remaining.id) return false
        dirty = false; saveFailed = false
        closeDecision.close()
        hide()
        return true
    }
    function showToolsReview(mode) {
        if (!mail.demo) return
        toolsReview.mode = mode
        toolsReview.restart()
    }
    Timer {
        id: toolsReview
        property string mode: ""
        interval: 350
        onTriggered: {
            if (!root.mail.demo || !root.visible) return
            if (mode === "emoji") emojiPicker.open()
            else if (mode === "font") fontFamilySelect.openMenu()
        }
    }
    function refreshParagraphState() {
        richEditor.setSelection(bodyField.selectionStart, bodyField.selectionEnd)
        paragraphState = richEditor.blockState(bodyField.textDocument)
    }
    function insertText(value) {
        if (!editable) return
        richEditor.setSelection(bodyField.selectionStart, bodyField.selectionEnd)
        bodyField.cursorPosition = richEditor.insertText(bodyField.textDocument, value)
        bodyField.forceActiveFocus()
    }
    function applyStyle(index) {
        format(() => {
            const selection = bodyField.cursorSelection
            selection.font.bold = index > 0
            selection.font.italic = false
            selection.font.underline = false
            selection.font.strikeout = false
            selection.font.pointSize = [12, 24, 18][index]
            selection.font.family = "sans-serif"
        })
    }
    function format(action) {
        if (!editable) return
        richEditor.setSelection(bodyField.selectionStart, bodyField.selectionEnd)
        action()
        refreshParagraphState()
        changed()
        bodyField.forceActiveFocus()
    }
    onClosing: event => { event.accepted = false; requestClose() }
    Timer { id: autosave; interval: 800; onTriggered: root.persist() }
    RichTextEditor { id: richEditor }
    Connections {
        target: root.mail
        function onDraftsChanged() {
            if (!root.visible || !root.draft.id || !root.accountMatches) return
            const latest = root.mail.loadDraft(root.draft.id)
            if (!latest || !latest.id) return
            const updated = root.copy(root.draft)
            updated.state = latest.state
            root.draft = updated
        }
        function onDraftAccepted(id) {
            if (String(root.draft.id) === String(id)) {
                autosave.stop()
                root.dirty = false
                closeDecision.close()
                root.hide()
            }
        }
    }
    Shortcut { sequence: "Escape"; enabled: root.active && !closeDecision.visible; onActivated: root.requestClose() }
    Shortcut { sequence: "Ctrl+W"; enabled: root.active && !closeDecision.visible; onActivated: root.requestClose() }
    Shortcut { sequence: "Ctrl+S"; enabled: root.active && root.editable && !closeDecision.visible; onActivated: root.persist() }
    Shortcut { sequence: "Ctrl+B"; enabled: root.active && bodyField.activeFocus && root.editable && !closeDecision.visible; onActivated: root.format(() => bodyField.cursorSelection.font.bold = !bodyField.cursorSelection.font.bold) }
    Shortcut { sequence: "Ctrl+I"; enabled: root.active && bodyField.activeFocus && root.editable && !closeDecision.visible; onActivated: root.format(() => bodyField.cursorSelection.font.italic = !bodyField.cursorSelection.font.italic) }
    Shortcut { sequence: "Ctrl+U"; enabled: root.active && bodyField.activeFocus && root.editable && !closeDecision.visible; onActivated: root.format(() => bodyField.cursorSelection.font.underline = !bodyField.cursorSelection.font.underline) }
    Shortcut { sequence: "Ctrl+K"; enabled: root.active && bodyField.activeFocus && root.editable && bodyField.selectedText.length > 0 && !closeDecision.visible; onActivated: { linkUrl.text = ""; linkEditor.open() } }

    Item {
        id: composeScene
        anchors.fill: parent
        // Same ownership as Main.qml: flush content planes, no client-side
        // window radius, clipping, outline or decoration shadow.
        Item {
            id: composerPlanes
            objectName: "composerPlanes"
            anchors.fill: parent
            Rectangle { objectName: "composerSurface"; anchors.fill: parent; color: AppTheme.contentBackground; border.width: 0 }
        }
        LiquidBackdrop {
            id: composerBackdrop
            objectName: "composerBackdrop"
            sourceItem: composerPlanes
            registerAsDefault: false
            active: root.liquidControls
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Item {
                Layout.fillWidth: true; Layout.preferredHeight: MailStyle.shellHeaderHeight
                WindowChrome {
                    id: composerChrome
                    objectName: "composerWindowChrome"
                    anchors.fill: parent
                    targetWindow: root
                }
                RowLayout {
                    anchors.left: parent.left; anchors.leftMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10
                    Item {
                        implicitWidth: headerActions.implicitWidth; implicitHeight: headerActions.implicitHeight
                        LiquidActionButton {
                            objectName: "composerActionGroupSurface"
                            anchors.fill: parent
                            materialForm: !root.liquidControls
                            backdrop: composerBackdrop
                            enabled: false; Accessible.ignored: true
                        }
                        Row {
                            id: headerActions
                            HeaderAction { glyph: "undo"; flat: true; text: qsTr("Undo"); enabled: root.editable && bodyField.canUndo; onClicked: bodyField.undo() }
                            HeaderAction { glyph: "redo"; flat: true; text: qsTr("Redo"); enabled: root.editable && bodyField.canRedo; onClicked: bodyField.redo() }
                            HeaderAction { glyph: "format"; objectName: "composerFormattingToggle"; flat: true; accessibleText: qsTr("Formatting options"); onClicked: root.showFormatting = !root.showFormatting }
                            HeaderAction { id: emojiButton; glyph: "emoji"; objectName: "composerEmoji"; flat: true; accessibleText: qsTr("Insert emoji"); enabled: root.editable; onClicked: emojiPicker.open() }
                            HeaderAction { id: linkButton; glyph: "link"; objectName: "composerLink"; flat: true; accessibleText: qsTr("Insert link"); enabled: root.editable && bodyField.selectedText.length > 0; onClicked: { linkUrl.text = ""; linkEditor.open() } }
                        }
                    }
                    HeaderAction {
                        glyph: "attach"; text: qsTr("Attach file…")
                        floatingAction: true; shellBackdrop: composerBackdrop
                        enabled: root.editable
                        onClicked: attachmentDialog.open()
                    }
                    HeaderAction { glyph: "recipients"; floatingAction: true; shellBackdrop: composerBackdrop; text: qsTr("Bcc"); enabled: root.editable; onClicked: root.showBcc = !root.showBcc }
                }
                MailActivityIndicator {
                    objectName: "sendingSpinner"
                    x: composerChrome.controlsLeft - 66
                    y: composerChrome.controlsCenterY - height / 2
                    width: 20; height: 20
                    visible: root.mail.sending
                    running: root.mail.sending
                }
                HeaderAction {
                    objectName: "sendDraftButton"
                    glyph: root.mail.sendPending ? "undo" : "send"
                    // Match the inbox search action: 10px from the visible
                    // first traffic-light circle, rather than its reserved area.
                    x: composerChrome.controlsLeft - width - 10
                    y: composerChrome.controlsCenterY - height / 2
                    implicitWidth: 28; implicitHeight: 28
                    floatingAction: true; shellBackdrop: composerBackdrop
                    text: root.mail.sendPending ? qsTr("撤销发送") : root.mail.sending ? qsTr("Sending…") : qsTr("Send")
                    enabled: root.mail.sendPending ? true : (root.editable && !root.mail.busy && !root.mail.demo && root.mail.configured
                        && (toField.text.trim().length > 0 || ccField.text.trim().length > 0 || bccField.text.trim().length > 0)
                        && ["unknown", "accepted", "sending"].indexOf(root.draft.state) < 0)
                    onClicked: { if (root.mail.sendPending) root.mail.cancelSend(); else if (root.persist()) root.mail.sendDraft(root.currentDraft()) }
                }
            }
            Rule {}
            RowLayout {
                Layout.fillWidth: true; Layout.leftMargin: 26; Layout.rightMargin: 26
                spacing: 6
                FieldLabel { text: qsTr("To") + ":" }
                AddressField { id: toField; objectName: "composerTo"; Accessible.name: qsTr("To"); placeholderText: qsTr("Recipient address") }
            }
            Rule {}
            RowLayout {
                Layout.fillWidth: true; Layout.leftMargin: 26; Layout.rightMargin: 26
                spacing: 6
                FieldLabel { text: qsTr("Cc") + ":" }
                AddressField { id: ccField; Accessible.name: qsTr("Cc") }
            }
            Rule {}
            RowLayout {
                visible: root.showBcc
                Layout.fillWidth: true; Layout.leftMargin: 26; Layout.rightMargin: 26
                spacing: 6
                FieldLabel { text: qsTr("Bcc") + ":" }
                AddressField { id: bccField; Accessible.name: qsTr("Bcc") }
            }
            Rule { visible: root.showBcc }
            RowLayout {
                Layout.fillWidth: true; Layout.leftMargin: 26; Layout.rightMargin: 26
                spacing: 6
                FieldLabel { text: qsTr("Subject") + ":" }
                AddressField { id: subjectField; objectName: "composerSubject"; Accessible.name: qsTr("Subject") }
            }
            Rule {}
            RowLayout {
                Layout.fillWidth: true; Layout.leftMargin: 26; Layout.rightMargin: 26
                Layout.preferredHeight: 32
                FieldLabel { text: qsTr("From") + ":" }
                Text {
                    text: String(root.fromAddress || qsTr("Example account"))
                    color: AppTheme.mutedText; font.pixelSize: 12
                    Layout.fillWidth: true; elide: Text.ElideRight
                    textFormat: Text.PlainText
                    Accessible.name: qsTr("From address")
                }
            }
            Flickable {
                visible: root.showFormatting
                Layout.fillWidth: true; Layout.preferredHeight: 46
                Layout.leftMargin: 24; Layout.rightMargin: 24
                contentWidth: formatRow.implicitWidth
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                RowLayout {
                    id: formatRow
                    height: parent.height
                    spacing: 7
                    enabled: root.editable
                    FormatSelect {
                        objectName: "composerParagraphStyle"
                        implicitWidth: 104; implicitHeight: 24
                        model: [qsTr("Body"), qsTr("Title"), qsTr("Heading")]
                        currentIndex: bodyField.cursorSelection.font.bold && bodyField.cursorSelection.font.pointSize === 24 ? 1
                            : bodyField.cursorSelection.font.bold && bodyField.cursorSelection.font.pointSize === 18 ? 2 : 0
                        Accessible.name: qsTr("Paragraph style")
                        onActivated: index => root.applyStyle(index)
                    }
                    FormatSelect {
                        id: fontFamilySelect
                        objectName: "composerFontFamily"
                        implicitWidth: 136; implicitHeight: 24
                        model: root.availableFonts
                        currentIndex: Math.max(0, root.availableFonts.indexOf(bodyField.cursorSelection.font.family))
                        placeholder: qsTr("System font")
                        Accessible.name: qsTr("Font family")
                        onActivated: index => root.format(() => bodyField.cursorSelection.font.family = root.availableFonts[index])
                    }
                    FormatSelect {
                        objectName: "composerFontStyle"
                        implicitWidth: 104; implicitHeight: 24
                        model: [qsTr("Regular"), qsTr("Bold"), qsTr("Italic"), qsTr("Bold italic")]
                        currentIndex: (bodyField.cursorSelection.font.bold ? 1 : 0) + (bodyField.cursorSelection.font.italic ? 2 : 0)
                        Accessible.name: qsTr("Font style")
                        onActivated: index => root.format(() => {
                            bodyField.cursorSelection.font.bold = (index & 1) !== 0
                            bodyField.cursorSelection.font.italic = (index & 2) !== 0
                        })
                    }
                    FormatSelect {
                        objectName: "composerFontSize"
                        implicitWidth: 64; implicitHeight: 24
                        model: ["10", "12", "14", "16", "18", "24", "32"]
                        currentIndex: model.indexOf(String(Math.round(bodyField.cursorSelection.font.pointSize)))
                        placeholder: String(Math.round(bodyField.cursorSelection.font.pointSize > 0 ? bodyField.cursorSelection.font.pointSize : 12))
                        Accessible.name: qsTr("Font size")
                        onActivated: index => root.format(() => bodyField.cursorSelection.font.pointSize = Number(model[index]))
                    }
                    FormatAction {
                        text: qsTr("Text color"); implicitWidth: 24; implicitHeight: 24; flat: false
                        contentItem: Item {
                            Rectangle { anchors.centerIn: parent; width: 16; height: 16; color: bodyField.cursorSelection.color; radius: 6; border.width: 1; border.color: AppTheme.border }
                        }
                        onClicked: { colorDialog.selectedColor = bodyField.cursorSelection.color; colorDialog.open() }
                    }
                    FormatGroup {
                        FormatAction { objectName: "composerBold"; flat: true; text: "B"; accessibleText: qsTr("Bold"); checkable: true; checked: bodyField.cursorSelection.font.bold; font.bold: true; onClicked: root.format(() => bodyField.cursorSelection.font.bold = !bodyField.cursorSelection.font.bold) }
                        FormatAction { flat: true; text: "I"; accessibleText: qsTr("Italic"); checkable: true; checked: bodyField.cursorSelection.font.italic; font.italic: true; onClicked: root.format(() => bodyField.cursorSelection.font.italic = !bodyField.cursorSelection.font.italic) }
                        FormatAction { flat: true; text: "U"; accessibleText: qsTr("Underline"); checkable: true; checked: bodyField.cursorSelection.font.underline; font.underline: true; onClicked: root.format(() => bodyField.cursorSelection.font.underline = !bodyField.cursorSelection.font.underline) }
                        FormatAction { flat: true; text: "S"; accessibleText: qsTr("Strikethrough"); checkable: true; checked: bodyField.cursorSelection.font.strikeout; font.strikeout: true; onClicked: root.format(() => bodyField.cursorSelection.font.strikeout = !bodyField.cursorSelection.font.strikeout) }
                    }
                    FormatGroup {
                        AlignmentAction { objectName: "composerAlign0"; paragraphAlignment: Qt.AlignLeft; accessibleText: qsTr("Align left") }
                        AlignmentAction { objectName: "composerAlign1"; paragraphAlignment: Qt.AlignHCenter; accessibleText: qsTr("Align center") }
                        AlignmentAction { objectName: "composerAlign2"; paragraphAlignment: Qt.AlignRight; accessibleText: qsTr("Align right") }
                    }
                    FormatGroup {
                        ListAction { objectName: "composerBullets"; accessibleText: qsTr("Bullet list"); checkable: true; checked: root.paragraphState.bullets; onClicked: root.format(() => richEditor.toggleBulletList(bodyField.textDocument)) }
                        ListAction { objectName: "composerNumbers"; numbered: true; accessibleText: qsTr("Numbered list"); checkable: true; checked: root.paragraphState.numbered; onClicked: root.format(() => richEditor.toggleNumberedList(bodyField.textDocument)) }
                    }
                    FormatGroup {
                        FormatAction { objectName: "composerOutdent"; text: "⇤"; accessibleText: qsTr("Decrease indent"); onClicked: root.format(() => richEditor.changeIndent(bodyField.textDocument, -1)) }
                        FormatAction { objectName: "composerIndent"; text: "⇥"; accessibleText: qsTr("Increase indent"); onClicked: root.format(() => richEditor.changeIndent(bodyField.textDocument, 1)) }
                    }
                }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.leftMargin: 28; Layout.rightMargin: 28
                Layout.topMargin: 12; Layout.bottomMargin: 20
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                TextArea {
                    id: bodyField
                    objectName: "composerBody"
                    enabled: root.editable
                    color: AppTheme.text
                    selectionColor: AppTheme.accent; selectedTextColor: AppTheme.accentText
                    placeholderText: qsTr("Write something thoughtful…")
                    placeholderTextColor: AppTheme.mutedText
                    font.pointSize: 12
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.RichText
                    selectByMouse: true
                    persistentSelection: true
                    Accessible.name: qsTr("Message body")
                    padding: 0
                    background: null
                    onTextChanged: { root.changed(); root.refreshParagraphState() }
                    onCursorPositionChanged: root.refreshParagraphState()
                    onSelectionStartChanged: root.refreshParagraphState()
                    onSelectionEndChanged: root.refreshParagraphState()
                }
            }
            Repeater {
                model: root.draft.attachments || []
                delegate: RowLayout {
                    id: attachment
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true; Layout.leftMargin: 26; Layout.rightMargin: 26
                    Text { text: attachment.modelData.name; color: AppTheme.text; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true; textFormat: Text.PlainText }
                    ComposerAction {
                        implicitHeight: 28; iconOnly: true; iconName: "close"; text: qsTr("Remove draft attachment")
                        enabled: root.editable
                        onClicked: {
                            const next = root.copy(root.draft)
                            next.attachments.splice(attachment.index, 1)
                            root.draft = next; root.changed()
                        }
                    }
                }
            }
            Text {
                visible: root.saveFailed || !!root.mail.error || root.draft.state === "unknown" || !root.accountMatches
                text: !root.accountMatches ? qsTr("Switch back to this draft's account to continue editing.")
                    : root.saveFailed ? root.localStatus : root.mail.error || qsTr("The server response was uncertain. Verify your Sent mailbox before sending again.")
                color: AppTheme.destructive; font.pixelSize: 12
                Layout.fillWidth: true; Layout.margins: 16
                wrapMode: Text.Wrap; textFormat: Text.PlainText
            }
        }
        DropArea {
            objectName: "attachmentDropArea"
            anchors.fill: parent
            enabled: root.editable && !closeDecision.visible
            onEntered: drag => { drag.accepted = drag.hasUrls }
            onDropped: drop => {
                if (!drop.hasUrls || !root.editable) return
                let added = 0
                for (const url of drop.urls) {
                    const item = root.mail.addAttachment(String(url))
                    if (!item || !item.path) continue
                    const next = root.copy(root.draft)
                    next.attachments = (next.attachments || []).concat([item])
                    root.draft = next
                    added++
                }
                if (added > 0) { root.changed(); root.localStatus = qsTr("Attached %1 file(s)").arg(added) }
            }
        }
    }
    KosWindowResizeHandles { targetWindow: root }
    LiquidDialog {
        id: closeDecision
        objectName: "composerCloseDecision"
        parent: Overlay.overlay
        sourceItem: composeScene
        width: Math.min(360, root.width - 48)
        anchors.centerIn: parent
        title: qsTr("Save this message as a draft?")
        closePolicy: Popup.NoAutoClose
        contentItem: ColumnLayout {
            spacing: 8
            Text {
                Layout.fillWidth: true; Layout.bottomMargin: 12
                text: qsTr("This message has not been sent. Save it as a draft to continue writing later.")
                color: AppTheme.text; font.pixelSize: 14
                wrapMode: Text.Wrap; textFormat: Text.PlainText
            }
            Text {
                visible: root.saveFailed || !!root.mail.error
                Layout.fillWidth: true
                text: root.saveFailed ? root.localStatus : root.mail.error
                color: AppTheme.destructive
                wrapMode: Text.Wrap; textFormat: Text.PlainText
            }
            LiquidDialogButton {
                objectName: "composerSaveAndClose"
                Layout.fillWidth: true; implicitHeight: 30
                text: qsTr("Save"); primaryAction: true
                enabled: root.accountMatches && !root.mail.sending && !root.mail.sendPending
                onClicked: root.finishEditing()
            }
            LiquidDialogButton {
                objectName: "composerDiscardAndClose"
                Layout.fillWidth: true; implicitHeight: 30
                text: qsTr("Don't Save"); destructive: true
                enabled: root.editable
                onClicked: root.discardAndClose()
            }
            LiquidDialogButton {
                objectName: "composerCancelClose"
                Layout.fillWidth: true; implicitHeight: 30
                text: qsTr("Cancel")
                onClicked: { closeDecision.close(); if (root.dirty) autosave.restart() }
            }
        }
        footer: Item {}
        Shortcut { sequence: "Escape"; enabled: closeDecision.visible; onActivated: { closeDecision.close(); if (root.dirty) autosave.restart() } }
    }
    LiquidContentPopover {
        id: emojiPicker
        objectName: "composerEmojiPicker"
        anchorItem: emojiButton; sourceItem: composeScene; preferredSide: "bottom"
        width: 250
        implicitHeight: emojiGrid.implicitHeight + topPadding + bottomPadding
        contentItem: GridLayout {
            id: emojiGrid
            columns: 6; rowSpacing: 4; columnSpacing: 4
            Repeater {
                objectName: "composerEmojiChoices"
                model: ["😀", "😊", "😉", "😍", "🤔", "😅", "🎉", "👍", "👏", "🙏", "❤️", "✨", "✅", "📌", "📅", "💡", "☕", "🌷"]
                delegate: ComposerAction {
                    required property string modelData
                    text: modelData; implicitWidth: 32; implicitHeight: 32
                    iconOnly: true
                    leftPadding: 0; rightPadding: 0; topPadding: 0; bottomPadding: 0
                    contentItem: Text {
                        text: parent.text
                        font.family: "Noto Color Emoji"
                        font.pixelSize: 22
                        font.preferShaping: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideNone
                    }
                    enabled: root.editable
                    onClicked: { root.insertText(modelData); emojiPicker.close() }
                }
            }
        }
    }
    LiquidContentPopover {
        id: linkEditor
        objectName: "composerLinkEditor"
        anchorItem: linkButton; sourceItem: composeScene; preferredSide: "bottom"
        width: Math.min(340, root.width - 16)
        implicitHeight: linkColumn.implicitHeight + topPadding + bottomPadding
        onOpened: linkUrl.forceActiveFocus()
        contentItem: ColumnLayout {
            id: linkColumn
            Text { text: qsTr("Link address"); color: AppTheme.text }
            TextField {
                id: linkUrl
                objectName: "composerLinkUrl"
                Layout.fillWidth: true
                placeholderText: "https://example.com"
                Accessible.name: qsTr("Link address")
                selectByMouse: true
                background: Rectangle { color: AppTheme.fieldSurface; radius: AppTheme.smallRadius }
            }
            ComposerAction {
                objectName: "composerInsertLink"
                text: qsTr("Insert link")
                enabled: root.editable && /^(https?:\/\/[^\s]+|mailto:[^\s@]+@[^\s@]+)$/i.test(linkUrl.text.trim())
                onClicked: {
                    root.format(() => richEditor.insertLink(bodyField.textDocument, linkUrl.text.trim()))
                    linkEditor.close()
                }
            }
        }
    }
    ColorDialog {
        id: colorDialog
        selectedColor: AppTheme.text
        title: qsTr("Text color")
        onAccepted: root.format(() => bodyField.cursorSelection.color = selectedColor)
    }
    FileDialog {
        id: attachmentDialog
        title: qsTr("Attach a file")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            if (!root.editable) return
            const item = root.mail.addAttachment(selectedFile.toString())
            if (!item || !item.path) return
            const next = root.copy(root.draft)
            next.attachments = (next.attachments || []).concat([item])
            root.draft = next; root.changed()
        }
    }
}

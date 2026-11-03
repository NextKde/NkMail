import QtQuick
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
    readonly property bool accountMatches: !draft.accountId || draft.accountId === mail.currentAccountId
    readonly property bool editable: accountMatches && !mail.sending && !mail.sendPending
    objectName: "composerDialog" // Preserve the public review/test lookup.
    width: 920; height: 740
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
    function format(action) {
        if (!editable) return
        richEditor.setSelection(bodyField.selectionStart, bodyField.selectionEnd)
        action()
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
                            ComposerAction { flat: true; text: qsTr("Undo"); iconOnly: true; iconName: "undo"; enabled: root.editable && bodyField.canUndo; onClicked: bodyField.undo() }
                            ComposerAction { flat: true; text: qsTr("Redo"); iconOnly: true; iconName: "forward"; enabled: root.editable && bodyField.canRedo; onClicked: bodyField.redo() }
                            ComposerAction { flat: true; text: "Aa"; Accessible.name: qsTr("Formatting options"); onClicked: root.showFormatting = !root.showFormatting }
                        }
                    }
                    ComposerAction {
                        text: qsTr("Attach file…"); iconOnly: true
                        floatingAction: true; shellBackdrop: composerBackdrop
                        enabled: root.editable
                        contentItem: Item {
                            Image {
                                anchors.centerIn: parent
                                width: 19; height: 19
                                sourceSize: Qt.size(36, 36)
                                fillMode: Image.PreserveAspectFit
                                source: "data:image/svg+xml;utf8," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="none" stroke="' + (root.editable ? AppTheme.text : AppTheme.disabledText) + '" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" d="m21 11-9 9a6 6 0 0 1-8.5-8.5l9-9a4 4 0 0 1 5.7 5.7l-9 9a2 2 0 0 1-2.8-2.8L15 6"/></svg>')
                            }
                        }
                        onClicked: attachmentDialog.open()
                    }
                    ComposerAction { floatingAction: true; shellBackdrop: composerBackdrop; text: qsTr("Bcc"); checkable: true; checked: root.showBcc; enabled: root.editable; onClicked: root.showBcc = !root.showBcc }
                }
                ComposerAction {
                    objectName: "sendDraftButton"
                    anchors.right: parent.right
                    anchors.rightMargin: composerChrome.controlsReservedWidth + 12
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 28; implicitHeight: 28
                    floatingAction: true; shellBackdrop: composerBackdrop
                    text: root.mail.sendPending ? qsTr("撤销发送") : root.mail.sending ? qsTr("Sending…") : qsTr("Send")
                    enabled: root.mail.sendPending ? true : (root.editable && !root.mail.busy && !root.mail.demo && root.mail.configured
                        && (toField.text.trim().length > 0 || ccField.text.trim().length > 0 || bccField.text.trim().length > 0)
                        && ["unknown", "accepted", "sending"].indexOf(root.draft.state) < 0)
                    contentItem: Text {
                        text: root.mail.sendPending ? "↶" : "↑"
                        font.pixelSize: 21
                        color: parent.textColor
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
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
                RowLayout {
                    id: formatRow
                    height: parent.height
                    spacing: 7
                    enabled: root.editable
                    KosComboBox {
                        objectName: "composerFontFamily"
                        implicitWidth: 150; implicitHeight: 30
                        model: [qsTr("System font"), qsTr("Serif"), qsTr("Monospace")]
                        Accessible.name: qsTr("Font family")
                        onActivated: index => root.format(() => bodyField.cursorSelection.font.family = ["sans-serif", "serif", "monospace"][index])
                    }
                    KosComboBox {
                        objectName: "composerFontSize"
                        implicitWidth: 72; implicitHeight: 30
                        model: ["10", "12", "14", "16", "18", "24", "32"]
                        currentIndex: 2
                        Accessible.name: qsTr("Font size")
                        onActivated: root.format(() => bodyField.cursorSelection.font.pointSize = Number(currentText))
                    }
                    ComposerAction {
                        text: qsTr("Text color"); implicitWidth: 30; implicitHeight: 30
                        contentItem: Item {
                            Rectangle { anchors.centerIn: parent; width: 22; height: 22; color: colorDialog.selectedColor; radius: 6; border.width: 1; border.color: AppTheme.border }
                        }
                        onClicked: colorDialog.open()
                    }
                    Item {
                        implicitWidth: styleActions.implicitWidth; implicitHeight: 32
                        LiquidActionButton { anchors.fill: parent; materialForm: true; enabled: false; Accessible.ignored: true }
                        Row {
                            id: styleActions
                            spacing: 1
                            ComposerAction { objectName: "composerBold"; flat: true; text: "B"; Accessible.name: qsTr("Bold"); font.bold: true; onClicked: root.format(() => bodyField.cursorSelection.font.bold = !bodyField.cursorSelection.font.bold) }
                            ComposerAction { flat: true; text: "I"; Accessible.name: qsTr("Italic"); font.italic: true; onClicked: root.format(() => bodyField.cursorSelection.font.italic = !bodyField.cursorSelection.font.italic) }
                            ComposerAction { flat: true; text: "U"; Accessible.name: qsTr("Underline"); font.underline: true; onClicked: root.format(() => bodyField.cursorSelection.font.underline = !bodyField.cursorSelection.font.underline) }
                            ComposerAction { flat: true; text: "S"; Accessible.name: qsTr("Strikethrough"); font.strikeout: true; onClicked: root.format(() => bodyField.cursorSelection.font.strikeout = !bodyField.cursorSelection.font.strikeout) }
                        }
                    }
                    ComposerAction { text: "• ≡"; Accessible.name: qsTr("Bullet list"); onClicked: root.format(() => richEditor.toggleBulletList(bodyField.textDocument)) }
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
                    font.pixelSize: 14
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.RichText
                    selectByMouse: true
                    persistentSelection: true
                    Accessible.name: qsTr("Message body")
                    padding: 0
                    background: null
                    onTextChanged: root.changed()
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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kos.Ui
import NextKde.Mail.Editor 1.0

LiquidDialog {
    id: root
    required property var mail
    required property var hostWindow
    property var draft: ({})
    property bool populating: false
    property bool dirty: false
    property bool saveFailed: false
    property string localStatus: ""
    property bool showCc: false
    property bool showBcc: false
    objectName: "composerDialog"
    parent: Overlay.overlay
    width: Math.min(760, hostWindow.width - 32)
    height: Math.min(690, hostWindow.height - 32)
    anchors.centerIn: parent
    title: qsTr("New message")
    closePolicy: Popup.NoAutoClose
    focus: true
    padding: 24
    materialOpacity: 0.99

    function copy(map) { return JSON.parse(JSON.stringify(map || {})) }
    function editDraft(value) {
        if (visible && !persist()) return false
        if (!value || !value.id) return false
        populating = true
        draft = copy(value)
        toField.text = String(draft.to || "")
        ccField.text = String(draft.cc || "")
        bccField.text = String(draft.bcc || "")
        subjectField.text = String(draft.subject || "")
        bodyField.text = draft.html ? String(draft.html) : String(draft.body || "")
        showCc = ccField.text.length > 0
        showBcc = bccField.text.length > 0
        dirty = false; saveFailed = false
        localStatus = qsTr("Saved locally")
        populating = false
        open()
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
    function finishEditing() {
        if (mail.sending || !persist()) return false
        close()
        return true
    }
    Timer { id: autosave; interval: 800; onTriggered: root.persist() }
    RichTextEditor { id: richEditor }
    Connections {
        target: root.mail
        function onDraftsChanged() {
            if (!root.visible || !root.draft.id) return
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
                root.close()
            }
        }
    }
    Shortcut { sequence: "Escape"; enabled: root.visible; onActivated: root.finishEditing() }
    Shortcut { sequence: "Ctrl+S"; enabled: root.visible && !root.mail.sending; onActivated: root.persist() }

    contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.bottomMargin: 10
            Text { text: qsTr("From"); color: AppTheme.mutedText; font.pixelSize: AppTheme.compactControlFontSize; Layout.preferredWidth: 56 }
            Text { text: String(root.mail.account.address || qsTr("Example account")); color: AppTheme.text; font.pixelSize: AppTheme.compactControlFontSize; Layout.fillWidth: true; elide: Text.ElideRight; Accessible.name: qsTr("From address") }
            MailAction { text: qsTr("Cc"); toolbarAction: false; checkable: true; checked: root.showCc; enabled: !root.mail.sending; Accessible.name: qsTr("Show carbon copy field"); onToggled: root.showCc = checked }
            MailAction { text: qsTr("Bcc"); toolbarAction: false; checkable: true; checked: root.showBcc; enabled: !root.mail.sending; Accessible.name: qsTr("Show blind carbon copy field"); onToggled: root.showBcc = checked }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2; columnSpacing: 10; rowSpacing: 8
            enabled: !root.mail.sending
            Text { text: qsTr("To"); color: AppTheme.mutedText; font.pixelSize: 12; Layout.preferredWidth: 54 }
            KosTextField { id: toField; objectName: "composerTo"; Layout.fillWidth: true; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; Accessible.name: qsTr("To"); placeholderText: qsTr("Recipient address"); onTextChanged: root.changed() }
            Text { visible: root.showCc; text: qsTr("Cc"); color: AppTheme.mutedText; font.pixelSize: 12 }
            KosTextField { id: ccField; visible: root.showCc; Layout.fillWidth: true; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; Accessible.name: qsTr("Cc"); onTextChanged: root.changed() }
            Text { visible: root.showBcc; text: qsTr("Bcc"); color: AppTheme.mutedText; font.pixelSize: 12 }
            KosTextField { id: bccField; visible: root.showBcc; Layout.fillWidth: true; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; Accessible.name: qsTr("Bcc"); onTextChanged: root.changed() }
            Text { text: qsTr("Subject"); color: AppTheme.mutedText; font.pixelSize: 12 }
            KosTextField { id: subjectField; objectName: "composerSubject"; Layout.fillWidth: true; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; Accessible.name: qsTr("Subject"); placeholderText: qsTr("Subject"); onTextChanged: root.changed() }
        }
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.topMargin: 14; Layout.bottomMargin: 10
            KosCard {
                anchors.fill: parent
                padding: 12
                background: KosSurface {
                    radius: AppTheme.smallRadius
                    fillColor: AppTheme.controlBackground
                    strokeColor: AppTheme.border
                    strokeWidth: 1
                    elevation: 0
                }
            }
            RowLayout {
                id: formatBar
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 8
                spacing: 4
                MailAction {
                    text: "B"; accessibleText: qsTr("Bold"); toolbarAction: false
                    implicitWidth: 26; implicitHeight: 22; font.pixelSize: 11; font.bold: true
                    enabled: !root.mail.sending
                    onClicked: { richEditor.toggleBold(bodyField.textDocument); root.changed() }
                }
                MailAction {
                    text: "I"; accessibleText: qsTr("Italic"); toolbarAction: false
                    implicitWidth: 26; implicitHeight: 22; font.pixelSize: 11; font.italic: true
                    enabled: !root.mail.sending
                    onClicked: { richEditor.toggleItalic(bodyField.textDocument); root.changed() }
                }
                MailAction {
                    text: "U"; accessibleText: qsTr("Underline"); toolbarAction: false
                    implicitWidth: 26; implicitHeight: 22; font.pixelSize: 11; font.underline: true
                    enabled: !root.mail.sending
                    onClicked: { richEditor.toggleUnderline(bodyField.textDocument); root.changed() }
                }
                MailAction {
                    text: qsTr("• 列表"); accessibleText: qsTr("Bullet list"); toolbarAction: false
                    implicitWidth: 56; implicitHeight: 22; font.pixelSize: 11
                    enabled: !root.mail.sending
                    onClicked: { richEditor.toggleBulletList(bodyField.textDocument); root.changed() }
                }
            }
            ScrollView {
                anchors.fill: parent; anchors.margins: 12; anchors.topMargin: 42
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                KosTextArea {
                    id: bodyField
                    objectName: "composerBody"
                    width: parent.width
                    enabled: !root.mail.sending
                    placeholderText: qsTr("Write something thoughtful…")
                    font.pixelSize: 14
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.RichText
                    selectByMouse: true
                    Accessible.name: qsTr("Message body")
                    padding: 0
                    background: null
                    onTextChanged: root.changed()
                }
            }
        }
        Repeater {
            model: root.draft.attachments || []
            delegate: RowLayout {
                id: attachment
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Text { text: attachment.modelData.name; color: AppTheme.text; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true; textFormat: Text.PlainText }
                MailAction {
                    implicitHeight: 24; iconOnly: true; iconName: "close"; accessibleText: qsTr("Remove draft attachment")
                    enabled: !root.mail.sending
                    onClicked: {
                        const next = root.copy(root.draft)
                        next.attachments.splice(attachment.index, 1)
                        root.draft = next; root.changed()
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            MailAction { text: qsTr("Attach file…"); iconName: ""; enabled: !root.mail.sending; onClicked: attachmentDialog.open() }
            Text {
                text: root.mail.demo ? qsTr("Demo · sending is disabled") : root.mail.sending ? qsTr("Submitting to SMTP…") : root.localStatus
                color: root.saveFailed ? AppTheme.destructive : AppTheme.mutedText
                font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignRight
            }
        }
        Text {
            visible: !!root.mail.error
            text: root.mail.error
            color: AppTheme.destructive; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
            Layout.topMargin: 8
        }
        Text {
            visible: root.draft.state === "unknown"
            text: qsTr("The server response was uncertain. Verify your Sent mailbox before sending again.")
            color: AppTheme.destructive; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
        }
    }
    footer: Item {
        implicitHeight: MailStyle.footerHeight
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 20; anchors.topMargin: 12; anchors.bottomMargin: 20
            spacing: 8
            Item { Layout.fillWidth: true }
            LiquidDialogButton { text: qsTr("Save draft & close"); enabled: !root.mail.sending; onClicked: root.finishEditing() }
            LiquidDialogButton {
                objectName: "sendDraftButton"
                text: root.mail.sending ? qsTr("Sending…") : qsTr("Send")
                primaryAction: true
                enabled: !root.mail.sending && !root.mail.busy && !root.mail.demo && root.mail.configured
                    && (toField.text.trim().length > 0 || ccField.text.trim().length > 0 || bccField.text.trim().length > 0)
                    && ["unknown", "accepted", "sending"].indexOf(root.draft.state) < 0
                onClicked: { if (root.persist()) root.mail.sendDraft(root.currentDraft()) }
            }
        }
    }
    FileDialog {
        id: attachmentDialog
        title: qsTr("Attach a file")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            const item = root.mail.addAttachment(selectedFile.toString())
            if (!item || !item.path) return
            const next = root.copy(root.draft)
            next.attachments = (next.attachments || []).concat([item])
            root.draft = next; root.changed()
        }
    }
}

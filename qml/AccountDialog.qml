import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

LiquidDialog {
    id: root
    required property var mail
    required property var hostWindow
    objectName: "accountDialog"
    parent: Overlay.overlay
    width: Math.min(610, hostWindow.width - 32)
    height: Math.min(730, hostWindow.height - 54)
    x: (hostWindow.width - width) / 2
    y: (hostWindow.height - height) / 2
    title: qsTr("Mail account")
    padding: 16
    property var original: ({})
    property bool submitting: false

    function field(map, key, fallback) {
        return map[key] !== undefined ? String(map[key]) : fallback || ""
    }
    function editAccount() {
        original = mail.account || ({})
        nameField.text = field(original, "name")
        addressField.text = field(original, "address")
        usernameField.text = field(original, "username")
        imapField.text = field(original, "imapHost")
        smtpField.text = field(original, "smtpHost")
        imapPortField.text = field(original, "imapPort", "993")
        smtpPortField.text = field(original, "smtpPort", "465")
        imapTls.checked = !!original.imapStartTls
        smtpTls.checked = !!original.smtpStartTls
        caField.text = field(original, "caFile")
        passwordField.text = ""
        rememberPassword.checked = false
        submitting = false
        open()
    }
    function valid() {
        return addressField.text.trim().length > 0 && usernameField.text.trim().length > 0
            && imapField.text.trim().length > 0 && smtpField.text.trim().length > 0
            && Number(imapPortField.text) > 0 && Number(imapPortField.text) <= 65535
            && Number(smtpPortField.text) > 0 && Number(smtpPortField.text) <= 65535
            && passwordField.text.length > 0
    }
    function save() {
        const config = {
            id: original.id || "", name: nameField.text.trim(), address: addressField.text.trim(),
            username: usernameField.text.trim(), imapHost: imapField.text.trim(), smtpHost: smtpField.text.trim(),
            imapPort: Number(imapPortField.text), smtpPort: Number(smtpPortField.text),
            imapStartTls: imapTls.checked, smtpStartTls: smtpTls.checked, caFile: caField.text.trim()
        }
        submitting = true
        mail.configureAccount(config, passwordField.text, rememberPassword.checked)
        passwordField.text = ""
        // Configuration may finish synchronously or asynchronously. Never claim authentication here.
        if (!mail.busy) {
            submitting = false
            if (!mail.error && mail.configured) close()
        }
    }
    onClosed: { passwordField.text = ""; submitting = false }
    Connections {
        target: root.mail
        function onStatusChanged() {
            if (root.submitting && !root.mail.busy) {
                root.submitting = false
                if (!root.mail.error && root.mail.configured) root.close()
            }
        }
    }

    contentItem: ScrollView {
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: parent.width
            spacing: 16
            Text {
                text: root.mail.demo ? qsTr("Example account · synthetic data only. Account changes and sending are disabled in demo mode.")
                    : qsTr("Connect one account with an application password. TLS certificate verification is always enabled.")
                color: AppTheme.mutedText; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            SettingsGroup {
                title: qsTr("Identity")
                SettingsItem { title: qsTr("Name"); KosTextField { id: nameField; objectName: "accountName"; implicitWidth: 265; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting } }
                SettingsItem { title: qsTr("Email"); KosTextField { id: addressField; implicitWidth: 265; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting; placeholderText: "you@example.invalid" } }
                SettingsItem { title: qsTr("Username"); KosTextField { id: usernameField; implicitWidth: 265; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting } }
                SettingsItem {
                    title: qsTr("App password"); description: qsTr("Stored passwords are never displayed.")
                    KosTextField { id: passwordField; objectName: "accountPassword"; implicitWidth: 265; implicitHeight: 28; echoMode: TextInput.Password; enabled: !root.mail.demo && !root.submitting; placeholderText: qsTr("Enter application password") }
                }
                SettingsItem {
                    title: qsTr("Remember password"); description: qsTr("Use the system keychain; no plaintext fallback.")
                    LiquidCheckBox { id: rememberPassword; enabled: !root.mail.demo && !root.submitting }
                }
            }
            SettingsGroup {
                title: qsTr("Incoming · IMAP")
                SettingsItem { title: qsTr("Server"); KosTextField { id: imapField; implicitWidth: 265; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting; placeholderText: "imap.example.invalid" } }
                SettingsItem { title: qsTr("Port"); KosTextField { id: imapPortField; implicitWidth: 90; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting; validator: IntValidator { bottom: 1; top: 65535 } } }
                SettingsItem {
                    title: qsTr("STARTTLS"); description: qsTr("Off uses implicit TLS, not plaintext.")
                    LiquidCheckBox { id: imapTls; enabled: !root.mail.demo && !root.submitting; onToggled: imapPortField.text = checked ? "143" : "993" }
                }
            }
            SettingsGroup {
                title: qsTr("Outgoing · SMTP")
                SettingsItem { title: qsTr("Server"); KosTextField { id: smtpField; implicitWidth: 265; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting; placeholderText: "smtp.example.invalid" } }
                SettingsItem { title: qsTr("Port"); KosTextField { id: smtpPortField; implicitWidth: 90; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting; validator: IntValidator { bottom: 1; top: 65535 } } }
                SettingsItem {
                    title: qsTr("STARTTLS"); description: qsTr("Off uses implicit TLS, not plaintext.")
                    LiquidCheckBox { id: smtpTls; enabled: !root.mail.demo && !root.submitting; onToggled: smtpPortField.text = checked ? "587" : "465" }
                }
            }
            SettingsGroup {
                title: qsTr("Trust")
                SettingsItem {
                    title: qsTr("CA file"); description: qsTr("Optional PEM path; blank uses system trust.")
                    KosTextField { id: caField; implicitWidth: 265; implicitHeight: 28; enabled: !root.mail.demo && !root.submitting; placeholderText: qsTr("System certificates") }
                }
            }
            Text { visible: !!root.mail.error; text: root.mail.error; color: AppTheme.destructive; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: qsTr("Mailboxes are read-only. Reading does not mark mail as read; move, delete and flag changes are not available."); color: AppTheme.mutedText; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
    }
    footer: Item {
        implicitHeight: 62
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 20; anchors.topMargin: 12; anchors.bottomMargin: 20
            spacing: 8
            LiquidDialogButton { text: qsTr("Disconnect"); visible: root.mail.configured && !root.mail.demo; enabled: !root.submitting && !root.mail.sending; onClicked: { root.mail.disconnectAccount(); root.close() } }
            Item { Layout.fillWidth: true }
            LiquidDialogButton { text: root.mail.demo ? qsTr("Done") : qsTr("Cancel"); enabled: !root.submitting; onClicked: root.close() }
            LiquidDialogButton {
                objectName: "connectAccountButton"
                visible: !root.mail.demo
                text: root.submitting ? qsTr("Saving…") : qsTr("Save account")
                primaryAction: true
                enabled: !root.submitting && !root.mail.busy && !root.mail.sending && root.valid()
                onClicked: root.save()
            }
        }
    }
}

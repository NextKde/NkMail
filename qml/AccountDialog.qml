import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui
import "ProviderProfiles.js" as Profiles

LiquidDialog {
    id: root
    required property var mail
    required property var hostWindow
    objectName: "accountDialog"
    parent: Overlay.overlay
    width: Math.min(580, hostWindow.width - 32)
    height: Math.min(header.implicitHeight + footer.implicitHeight
        + topPadding + bottomPadding + accountFields.implicitHeight, hostWindow.height - 32)
    anchors.centerIn: parent
    title: qsTr("Mail account")
    padding: 20
    topPadding: 4
    bottomPadding: 4
    header: Label {
        text: root.title
        leftPadding: 20; rightPadding: 20; topPadding: 16; bottomPadding: 12
        color: AppTheme.text
        font.pixelSize: 17
        font.weight: Font.DemiBold
    }
    materialOpacity: 0.99
    closePolicy: root.submitting ? Popup.NoAutoClose : Popup.CloseOnEscape
    property var original: ({})
    property bool submitting: false
    property int step: 0
    property bool advanced: false
    property string setupError: ""
    readonly property var setup: mail.accountSetup || null
    function nextStep() {
        if (!providerChoice.available) return
        usernameField.text = addressField.text.trim()
        const preset = Profiles.settings(providerChoice.provider)
        imapField.text = preset.imapHost; smtpField.text = preset.smtpHost
        imapPortField.text = String(preset.imapPort); smtpPortField.text = String(preset.smtpPort)
        imapTls.checked = preset.imapStartTls
        smtpTls.checked = preset.smtpStartTls
        serverSavesSent.checked = preset.serverSavesSent
        step = 1
    }
    function validateConnection() {
        if (!setup || submitting || !valid()) return
        setupError = ""; submitting = true
        setup.verify(configuration(), passwordField.text)
    }


    function field(map, key, fallback) {
        return map[key] !== undefined ? String(map[key]) : fallback || ""
    }
    function editAccount(account) {
        // No argument keeps the historical behaviour (edit the active account or
        // start a new one). A passed account map pre-fills those fields and keeps
        // its id in `original`, so save() edits that record instead of creating one.
        original = (account && String(account.id || "").length > 0) ? account : (mail.account || ({}))
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
        providerChoice.currentIndex = Profiles.indexForAddress(addressField.text)
        serverSavesSent.checked = !!original.serverSavesSent
        rememberPassword.checked = false
        submitting = false
        setupError = ""; advanced = false; step = 1
        open()
    }
    function addAccount() {
        editAccount({id: "new"})
        original = ({})
        step = 0
    }
    function valid() {
        return addressField.text.trim().length > 0 && usernameField.text.trim().length > 0
            && imapField.text.trim().length > 0 && smtpField.text.trim().length > 0
            && Number(imapPortField.text) > 0 && Number(imapPortField.text) <= 65535
            && Number(smtpPortField.text) > 0 && Number(smtpPortField.text) <= 65535
            && passwordField.text.length > 0
    }
    function configuration() {
        return {
            id: original.id || "", name: nameField.text.trim(), address: addressField.text.trim(),
            username: usernameField.text.trim(), imapHost: imapField.text.trim(), smtpHost: smtpField.text.trim(),
            imapPort: Number(imapPortField.text), smtpPort: Number(smtpPortField.text),
            imapStartTls: imapTls.checked, smtpStartTls: smtpTls.checked, serverSavesSent: serverSavesSent.checked, caFile: caField.text.trim()
        }
    }
    function save() {
        submitting = true
        const saved = mail.configureAccount(configuration(), passwordField.text, rememberPassword.checked)
        passwordField.text = ""
        // configureAccount returns its persistence result synchronously. A busy
        // mailbox can reject it; unrelated sync must never leave this dialog locked.
        submitting = false
        if (saved) step = 2
        else setupError = mail.error || qsTr("Account could not be saved. Please retry.")
    }
    onClosed: { passwordField.text = ""; submitting = false }
    Connections {
        target: root.setup
        function onFinished(success, error) {
            if (!root.submitting) return
            if (success) root.save()
            else { root.submitting = false; root.setupError = error; passwordField.text = "" }
        }
    }
    contentItem: ScrollView {
        id: accountScroll
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.vertical.interactive: true
        ColumnLayout {
            id: accountFields
            width: accountScroll.availableWidth
            spacing: 10
            Text {
                text: root.mail.demo ? qsTr("Example account · synthetic data only. Account changes and sending are disabled in demo mode.")
                    : qsTr("Connect one account with an application password. TLS certificate verification is always enabled.")
                color: AppTheme.mutedText; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Text {
                visible: root.step < 2
                text: root.step === 0 ? qsTr("Enter your email address") : qsTr("Email")
                Layout.fillWidth: true
                color: AppTheme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            KosTextField {
                id: addressField
                objectName: "accountAddress"
                visible: root.step < 2
                Layout.fillWidth: true
                implicitHeight: MailStyle.fieldHeight
                cornerRadius: height / 2
                specificationForm: true
                enabled: !root.submitting
                readOnly: root.submitting
                Accessible.name: qsTr("Email")
                placeholderText: qsTr("Enter you@example.com")
                onTextEdited: providerChoice.currentIndex = Profiles.indexForAddress(text)
            }
            SettingsGroup {
                visible: root.step === 1
                title: qsTr("Identity")
                SettingsItem { visible: root.step === 1; title: qsTr("Name"); KosTextField { id: nameField; objectName: "accountName"; Layout.fillWidth: true; implicitWidth: 260; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("Name") } }
                SettingsItem { visible: root.step === 1; title: qsTr("Username"); KosTextField { id: usernameField; Layout.fillWidth: true; implicitWidth: 260; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("Username") } }
                SettingsItem {
                    visible: root.step === 1; title: qsTr("App password"); description: qsTr("Stored passwords are never displayed.")
                    KosTextField { id: passwordField; objectName: "accountPassword"; Layout.fillWidth: true; implicitWidth: 260; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; echoMode: TextInput.Password; enabled: !root.mail.demo && !root.submitting; Accessible.name: qsTr("Application password"); placeholderText: qsTr("Enter application password") }
                }
                SettingsItem {
                    visible: root.step === 1; title: qsTr("Remember password"); description: qsTr("Use the system keychain; no plaintext fallback.")
                    KosCheckBox { id: rememberPassword; enabled: !root.mail.demo && !root.submitting }
                }
            }
            ProviderPicker {
                id: providerChoice
                objectName: "providerChoice"
                visible: root.step === 0
                Layout.fillWidth: true
                enabled: !root.submitting
            }
            Text {
                visible: root.step === 1
                text: providerChoice.guidance
                Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: AppTheme.mutedText
            }
            LiquidCheckBox {
                id: serverSavesSent
                visible: root.step === 1 && root.advanced
                text: qsTr("Server automatically saves sent mail")
                enabled: !root.submitting
            }
            LiquidCheckBox {
                visible: root.step === 1
                text: qsTr("Advanced server settings")
                checked: root.advanced
                enabled: !root.submitting
                onToggled: root.advanced = checked
            }
            Text {
                visible: root.step === 2
                text: qsTr("Account verified and saved")
                color: AppTheme.text; font.pixelSize: 18
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Text {
                visible: root.submitting
                text: root.setup && root.setup.stage === "smtp" ? qsTr("Checking outgoing server…") : qsTr("Checking incoming server…")
                color: AppTheme.mutedText; Layout.fillWidth: true
            }
            Text { visible: root.setupError.length > 0; text: root.setupError; color: AppTheme.destructive; Layout.fillWidth: true; wrapMode: Text.Wrap }
            SettingsGroup {
                visible: root.step === 1
                title: qsTr("Incoming · IMAP")
                SettingsItem { title: qsTr("Server"); KosTextField { id: imapField; Layout.fillWidth: true; implicitWidth: 260; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("IMAP server"); placeholderText: "imap.example.invalid" } }
                SettingsItem { visible: root.advanced; title: qsTr("Port"); KosTextField { id: imapPortField; implicitWidth: 90; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("IMAP port"); validator: IntValidator { bottom: 1; top: 65535 } } }
                SettingsItem {
                    visible: root.advanced; title: qsTr("STARTTLS"); description: qsTr("Off uses implicit TLS, not plaintext.")
                    KosCheckBox { id: imapTls; enabled: !root.mail.demo && !root.submitting; onToggled: imapPortField.text = checked ? "143" : "993" }
                }
            }
            SettingsGroup {
                visible: root.step === 1
                title: qsTr("Outgoing · SMTP")
                SettingsItem { title: qsTr("Server"); KosTextField { id: smtpField; Layout.fillWidth: true; implicitWidth: 260; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("SMTP server"); placeholderText: "smtp.example.invalid" } }
                SettingsItem { visible: root.advanced; title: qsTr("Port"); KosTextField { id: smtpPortField; implicitWidth: 90; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("SMTP port"); validator: IntValidator { bottom: 1; top: 65535 } } }
                SettingsItem {
                    visible: root.advanced; title: qsTr("STARTTLS"); description: qsTr("Off uses implicit TLS, not plaintext.")
                    KosCheckBox { id: smtpTls; enabled: !root.mail.demo && !root.submitting; onToggled: smtpPortField.text = checked ? "587" : "465" }
                }
            }
            SettingsGroup {
                visible: root.step === 1 && root.advanced
                title: qsTr("Trust")
                SettingsItem {
                    title: qsTr("CA file"); description: qsTr("Optional PEM path; blank uses system trust.")
                    KosTextField { id: caField; Layout.fillWidth: true; implicitWidth: 260; implicitHeight: MailStyle.fieldHeight; cornerRadius: height / 2; specificationForm: true; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("CA file"); placeholderText: qsTr("System certificates") }
                }
            }
            Text { visible: !!root.mail.error; text: root.mail.error; color: AppTheme.destructive; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: qsTr("Reading does not change server flags. Explicit mail actions can update or delete messages."); color: AppTheme.mutedText; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
    }
    footer: Item {
        implicitHeight: 54
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 20; anchors.topMargin: 8; anchors.bottomMargin: 16
            spacing: 8
            LiquidDialogButton { text: qsTr("Disconnect"); destructive: true; visible: root.mail.configured && !root.mail.demo; enabled: !root.submitting && !root.mail.sending; onClicked: { root.mail.disconnectAccount(); root.close() } }
            Item { Layout.fillWidth: true }
            LiquidDialogButton { text: root.mail.demo || root.step === 2 ? qsTr("Done") : qsTr("Cancel"); enabled: !root.submitting; onClicked: root.close() }
            LiquidDialogButton {
                text: qsTr("Back")
                visible: root.step === 1 && !root.original.id
                enabled: !root.submitting
                onClicked: root.step = 0
            }
            LiquidDialogButton {
                text: qsTr("Continue")
                visible: root.step === 0
                enabled: providerChoice.available && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(addressField.text.trim())
                onClicked: root.nextStep()
            }
            LiquidDialogButton {
                objectName: "connectAccountButton"
                visible: !root.mail.demo && root.step === 1
                text: root.submitting ? qsTr("Checking…") : qsTr("Verify and save")
                primaryAction: true
                enabled: !root.submitting && !root.mail.busy && !root.mail.sending && root.valid()
                onClicked: root.validateConnection()
            }
        }
    }
}

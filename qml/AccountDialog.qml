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
    closePolicy: root.submitting ? Popup.NoAutoClose : Popup.CloseOnEscape
    property var original: ({})
    property bool submitting: false
    property int step: 0
    property bool advanced: false
    property string incomingProtocol: "imap"
    function chooseProtocol(protocol) {
        incomingProtocol = protocol
        imapTls.checked = false
        imapPortField.text = protocol === "pop3" ? "995" : "993"
        imapField.text = protocol === "pop3" ? Profiles.popHost(providerChoice.provider) : Profiles.settings(providerChoice.provider).imapHost
    }
    property string setupError: ""
    readonly property var setup: mail.accountSetup || null
    function nextStep() {
        if (!providerChoice.available) return
        incomingProtocol = "imap"
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
        incomingProtocol = original.incomingProtocol || "imap"
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
        providerChoice.currentIndex = original.authMethod === "google_oauth" ? Profiles.ids.indexOf("google") : Profiles.indexForAddress(addressField.text)
        serverSavesSent.checked = !!original.serverSavesSent
        rememberPassword.checked = false
        submitting = false
        setupError = ""; advanced = false
        step = 1
        open()
    }
    // Synthetic capture helper, never available to real account sessions.
    function showGooglePasswordReview() {
        if (mail.demo) providerChoice.currentIndex = Profiles.ids.indexOf("google")
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
            incomingProtocol: incomingProtocol, username: usernameField.text.trim(), imapHost: imapField.text.trim(), smtpHost: smtpField.text.trim(),
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
    onClosed: {
        passwordField.text = ""; submitting = false
    }
    Connections {
        target: root.setup
        function onFinished(success, error) {
            if (!root.submitting) return
            if (success) {
                root.save()
            }
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
            spacing: 12
            Text {
                visible: root.step < 2
                text: root.mail.demo ? qsTr("Example account · synthetic data only. Account changes and sending are disabled in demo mode.")
                    : qsTr("Use the password or app password required by your email provider.")
                color: AppTheme.mutedText; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            SettingsGroup {
                visible: root.step < 2
                title: root.step === 0 ? qsTr("Enter your email address") : qsTr("Email")
                cornerRadius: 8
                SettingsTextField {
                    id: addressField
                    objectName: "accountAddress"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    blockInput: true
                    enabled: !root.submitting
                    readOnly: root.submitting
                    Accessible.name: qsTr("Email")
                    placeholderText: activeFocus ? "" : qsTr("Enter you@example.com")
                    onTextEdited: providerChoice.currentIndex = Profiles.indexForAddress(text)
                }
            }
            SettingsGroup {
                visible: root.step === 1
                title: qsTr("Identity")
                SettingsItem {
                    allowStacking: false; visible: root.step === 1; title: qsTr("Name"); SettingsTextField { id: nameField; objectName: "accountName"; Layout.fillWidth: true; Layout.preferredWidth: 260; Layout.minimumWidth: 0; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("Name") } }
                SettingsItem {
                    allowStacking: false; visible: root.step === 1; title: qsTr("Username"); SettingsTextField { id: usernameField; Layout.fillWidth: true; Layout.preferredWidth: 260; Layout.minimumWidth: 0; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("Username") } }
                SettingsItem {
                    allowStacking: false;
                    visible: root.step === 1; title: qsTr("Password / app password"); description: qsTr("Stored passwords are never displayed.")
                    SettingsTextField { id: passwordField; objectName: "accountPassword"; Layout.fillWidth: true; Layout.preferredWidth: 260; Layout.minimumWidth: 0; echoMode: TextInput.Password; enabled: !root.mail.demo && !root.submitting; Accessible.name: qsTr("Password or app password"); placeholderText: qsTr("Enter password or app password") }
                }
                SettingsItem {
                    allowStacking: false;
                    visible: root.step === 1; title: qsTr("Remember password"); description: qsTr("Use the system keychain; no plaintext fallback.")
                    SettingsSwitch { id: rememberPassword; objectName: "rememberPasswordSwitch"; accessibleName: qsTr("Remember password"); onToggled: checked => rememberPassword.checked = checked; enabled: !root.mail.demo && !root.submitting }
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
            Text {
                objectName: "googlePasswordMigrationHint"
                visible: root.step === 1 && root.original.authMethod === "google_oauth"
                text: qsTr("To switch this account to an app password, enter a Google app password below and verify. Cached mail is kept.")
                Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: AppTheme.mutedText
            }
            SettingsGroup {
                visible: root.step === 1
                title: qsTr("Connection options")
                SettingsItem {
                    title: qsTr("Advanced server settings")
                    SettingsSwitch {
                        objectName: "advancedSettingsSwitch"
                        accessibleName: qsTr("Advanced server settings")
                        checked: root.advanced
                        enabled: !root.submitting
                        onToggled: checked => root.advanced = checked
                    }
                }
                SettingsItem {
                    visible: root.advanced && root.incomingProtocol !== "pop3"
                    title: qsTr("Server automatically saves sent mail")
                    SettingsSwitch {
                        id: serverSavesSent
                        accessibleName: qsTr("Server automatically saves sent mail")
                        enabled: !root.submitting
                        onToggled: checked => serverSavesSent.checked = checked
                    }
                }
            }
            Text {
                visible: root.step === 2
                text: qsTr("Account verified and saved")
                color: AppTheme.text; font.pixelSize: 18
                Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            RowLayout {
                visible: root.submitting
                Layout.fillWidth: true
                spacing: 10
                MailActivityIndicator {
                    objectName: "accountSetupSpinner"
                    running: root.submitting
                    Layout.preferredWidth: 20; Layout.preferredHeight: 20
                }
                Text {
                    text: root.setup && root.setup.stage === "smtp" ? qsTr("Checking outgoing server…") : qsTr("Checking incoming server…")
                    color: AppTheme.mutedText; Layout.fillWidth: true; wrapMode: Text.Wrap
                }
            }
            Text { visible: root.setupError.length > 0; text: root.setupError; color: AppTheme.destructive; Layout.fillWidth: true; wrapMode: Text.Wrap }
            SettingsGroup {
                visible: root.step === 1
                title: root.incomingProtocol === "pop3" ? qsTr("Incoming · POP") : qsTr("Incoming · IMAP")
                SettingsItem {
                    allowStacking: false;
                    title: qsTr("Protocol")
                    description: root.incomingProtocol === "pop3" ? qsTr("POP keeps server copies. Read status, flags and deletion apply only on this device. Sent copies are saved locally; server folders are unavailable.") : ""
                    LiquidSelect {
                        implicitHeight: 24
                        materialForm: true
                        plainTrigger: true
                        indicatorStyle: "upDown"
                        indicatorBackground: true
                        popupMaterialForm: true
                        neutralOptionHighlight: true
                        compactPopup: true
                        popupBorderWidth: 0
                        popupShadowEnabled: true
                        popupFontWeight: Font.Normal
                        appearanceMode: AppTheme.dark ? "dark" : "light"
                        Accessible.name: qsTr("Protocol")
                        objectName: "incomingProtocolChoice"
                        model: providerChoice.provider === "icloud" ? ["IMAP"] : ["IMAP", "POP"]
                        currentIndex: root.incomingProtocol === "pop3" ? 1 : 0
                        enabled: !root.submitting && !root.original.id
                        onActivated: index => root.chooseProtocol(index === 1 ? "pop3" : "imap")
                    }
                }
                SettingsItem {
                    allowStacking: false; title: qsTr("Server"); SettingsTextField { id: imapField; Layout.fillWidth: true; Layout.preferredWidth: 260; Layout.minimumWidth: 0; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("Incoming server"); placeholderText: root.incomingProtocol === "pop3" ? "pop.example.invalid" : "imap.example.invalid" } }
                SettingsItem {
                    allowStacking: false; visible: root.advanced; title: qsTr("Port"); SettingsTextField { id: imapPortField; Layout.preferredWidth: 90; Layout.minimumWidth: 0; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("Incoming port"); validator: IntValidator { bottom: 1; top: 65535 } } }
                SettingsItem {
                    allowStacking: false;
                    visible: root.advanced; title: qsTr("STARTTLS"); description: qsTr("Off uses implicit TLS, not plaintext.")
                    SettingsSwitch { id: imapTls; accessibleName: qsTr("Incoming STARTTLS"); enabled: !root.mail.demo && !root.submitting; onToggled: checked => { imapTls.checked = checked; imapPortField.text = root.incomingProtocol === "pop3" ? (checked ? "110" : "995") : (checked ? "143" : "993") } }
                }
            }
            SettingsGroup {
                visible: root.step === 1
                title: qsTr("Outgoing · SMTP")
                SettingsItem {
                    allowStacking: false; title: qsTr("Server"); SettingsTextField { id: smtpField; Layout.fillWidth: true; Layout.preferredWidth: 260; Layout.minimumWidth: 0; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("SMTP server"); placeholderText: "smtp.example.invalid" } }
                SettingsItem {
                    allowStacking: false; visible: root.advanced; title: qsTr("Port"); SettingsTextField { id: smtpPortField; Layout.preferredWidth: 90; Layout.minimumWidth: 0; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("SMTP port"); validator: IntValidator { bottom: 1; top: 65535 } } }
                SettingsItem {
                    allowStacking: false;
                    visible: root.advanced; title: qsTr("STARTTLS"); description: qsTr("Off uses implicit TLS, not plaintext.")
                    SettingsSwitch { id: smtpTls; accessibleName: qsTr("Outgoing STARTTLS"); enabled: !root.mail.demo && !root.submitting; onToggled: checked => { smtpTls.checked = checked; smtpPortField.text = checked ? "587" : "465" } }
                }
            }
            SettingsGroup {
                visible: root.step === 1 && root.advanced
                title: qsTr("Trust")
                SettingsItem {
                    allowStacking: false;
                    title: qsTr("CA file"); description: qsTr("Optional PEM path; blank uses system trust.")
                    SettingsTextField { id: caField; Layout.fillWidth: true; Layout.preferredWidth: 260; Layout.minimumWidth: 0; enabled: !root.submitting; readOnly: root.mail.demo || root.submitting; Accessible.name: qsTr("CA file"); placeholderText: qsTr("System certificates") }
                }
            }
            Text { visible: !!root.mail.error; text: root.mail.error; color: AppTheme.destructive; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { visible: root.incomingProtocol !== "pop3"; text: qsTr("Reading does not change server flags. Explicit mail actions can update or delete messages."); color: AppTheme.mutedText; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
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
                objectName: "continueAccountButton"
                text: qsTr("Continue")
                visible: root.step === 0
                enabled: !root.submitting && !root.mail.busy && !root.mail.sending && providerChoice.available && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(addressField.text.trim())
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

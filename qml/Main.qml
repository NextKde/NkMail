import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import Kos.Ui

ApplicationWindow {
    id: window
    required property var mail
    objectName: "mailWindow"
    width: 1280
    height: 820
    minimumWidth: 580
    minimumHeight: 520
    visible: true
    title: qsTr("NkMail")
    flags: Qt.Window | Qt.FramelessWindowHint
    color: AppTheme.contentBackground
    font.family: "sans-serif"
    font.pixelSize: 13

    readonly property bool narrow: width < 800
    property bool sidebarExpanded: false
    property bool readerVisible: false
    property bool draftsVisible: false
    readonly property bool sidebarVisible: width >= 1080 || sidebarExpanded
    readonly property var message: mail.selectedMessage || ({})
    readonly property bool hasMessage: String(mail.selectedUid || "").length > 0
    readonly property color ink: AppTheme.dark ? "#ededf0" : "#242426"
    readonly property color muted: AppTheme.dark ? "#a1a1a8" : "#797980"
    readonly property color rule: AppTheme.dark ? "#353538" : "#e5e5e7"
    readonly property color sidebarColor: AppTheme.dark ? "#252527" : "#f0f0f2"

    function value(map, key) { return map && map[key] !== undefined ? String(map[key]) : "" }
    function folderTitle() {
        if (draftsVisible) return qsTr("Drafts")
        const folders = mail.folders || []
        for (let i = 0; i < folders.length; ++i)
            if (folders[i].id === mail.currentFolder) return folders[i].title
        return mail.currentFolder || qsTr("Inbox")
    }
    function dateLabel(date) {
        const d = new Date(date)
        if (isNaN(d.getTime())) return String(date || "")
        return Qt.formatDateTime(d, "MMM d, h:mm AP")
    }
    function compose(mode) {
        if (mail.sending) return
        composer.editDraft(mail.newDraft(mode || "new"))
    }
    // Capture helper is deliberately unavailable for real accounts.
    function showReview(scenario) {
        if (!mail.demo) return
        moreMenu.close(); details.close(); accountDialog.close()
        if (composer.visible && !composer.finishEditing()) return
        draftsVisible = false
        if (scenario === "compose") {
            const sample = mail.newDraft("new")
            if (!sample || !sample.id) return
            sample.to = "studio@example.invalid"
            sample.subject = qsTr("A few thoughts for Friday")
            sample.body = qsTr("Hi Maya,\n\nI like the quieter direction. A little more space to read, and fewer things competing for attention.\n\nLet's bring these ideas to our next review.\n\nTalk soon,\nAlex\n\n—\nSynthetic demo draft. Sending is disabled.")
            composer.editDraft(mail.saveDraft(sample))
        }
        else if (scenario === "account") accountDialog.editAccount()
        else if (scenario === "details" && hasMessage) details.open()
    }
    function toggleMaximized() {
        if (visibility === Window.Maximized) showNormal()
        else showMaximized()
    }
    Component.onCompleted: {
        AppTheme.compactDesign = true
        if (typeof reviewAppearance !== "undefined" && (reviewAppearance === "light" || reviewAppearance === "dark"))
            AppTheme.appearanceMode = reviewAppearance
    }
    onClosing: close => {
        if (composer.visible && (mail.sending || !composer.persist())) {
            close.accepted = false
            composer.forceActiveFocus()
        }
    }
    Connections {
        target: mail
        function onSelectionChanged() { if (window.narrow && window.hasMessage) window.readerVisible = true }
    }

    Item {
        id: mailScene
        objectName: "mailScene"
        anchors.fill: parent
        Rectangle { anchors.fill: parent; color: AppTheme.contentBackground }
        // Square desktop shell, with the same compact right-hand controls as KMusic.
        Rectangle {
            id: chrome
            height: 30
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            color: window.sidebarColor
            MouseArea {
                anchors.fill: parent; anchors.rightMargin: 110
                property point pressPoint
                property bool armed: false
                onPressed: mouse => { pressPoint = Qt.point(mouse.x, mouse.y); armed = true }
                onReleased: armed = false
                onCanceled: armed = false
                onPositionChanged: mouse => {
                    if (pressed && armed && (Math.abs(mouse.x - pressPoint.x) > 4 || Math.abs(mouse.y - pressPoint.y) > 4)) {
                        armed = false; window.startSystemMove()
                    }
                }
                onDoubleClicked: { armed = false; window.toggleMaximized() }
            }
            KosWindowControls {
                targetWindow: window
                maximizeMode: true
                circleSize: 14; buttonWidth: 14; buttonHeight: 14; spacing: 14
                anchors.right: parent.right; anchors.rightMargin: 15
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        RowLayout {
            anchors.top: chrome.bottom; anchors.bottom: statusStrip.top
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0
            Rectangle {
                id: sidebar
                visible: window.sidebarVisible
                Layout.preferredWidth: 236
                Layout.fillHeight: true
                color: window.sidebarColor
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true; Layout.preferredHeight: 42
                        Text { text: qsTr("Mail"); font.pixelSize: 24; font.weight: Font.Bold; color: window.ink; Layout.fillWidth: true }
                        MailAction {
                            iconOnly: true; iconName: "editRounded"; accessibleText: qsTr("New message")
                            objectName: "sidebarComposeButton"
                            enabled: mail.configured || mail.demo
                            onClicked: window.compose("new")
                        }
                    }
                    Text {
                        text: qsTr("MAILBOXES"); color: window.muted; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1
                        Layout.leftMargin: 9; Layout.topMargin: 18; Layout.bottomMargin: 8
                    }
                    Repeater {
                        model: mail.folders
                        delegate: AbstractButton {
                            id: folderButton
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: 34
                            hoverEnabled: true
                            Accessible.name: modelData.title
                            onClicked: {
                                window.draftsVisible = false; window.readerVisible = false
                                mail.selectFolder(modelData.id)
                                if (window.width < 1080) window.sidebarExpanded = false
                            }
                            background: Rectangle {
                                radius: 6
                                color: !window.draftsVisible && mail.currentFolder === folderButton.modelData.id
                                    ? (AppTheme.dark ? "#424247" : "#dcdce1")
                                    : folderButton.hovered ? (AppTheme.dark ? "#333337" : "#e7e7eb") : "transparent"
                            }
                            contentItem: RowLayout {
                                spacing: 10
                                VectorIcon { name: "folder"; size: 17; color: AppTheme.accent; Layout.leftMargin: 9 }
                                Text { text: folderButton.modelData.title; color: window.ink; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                Text {
                                    text: folderButton.modelData.total >= 0 ? String(folderButton.modelData.total) : ""
                                    color: window.muted; font.pixelSize: 11; Layout.rightMargin: 10
                                }
                            }
                        }
                    }
                    AbstractButton {
                        id: draftFolder
                        objectName: "draftFolderButton"
                        Layout.fillWidth: true; implicitHeight: 34
                        hoverEnabled: true
                        Accessible.name: qsTr("Local drafts")
                        onClicked: { window.draftsVisible = true; window.readerVisible = false; if (window.width < 1080) window.sidebarExpanded = false }
                        background: Rectangle { radius: 6; color: window.draftsVisible ? (AppTheme.dark ? "#424247" : "#dcdce1") : "transparent" }
                        contentItem: RowLayout {
                            spacing: 10
                            VectorIcon { name: "editRounded"; size: 17; color: AppTheme.accent; Layout.leftMargin: 9 }
                            Text { text: qsTr("Drafts"); color: window.ink; Layout.fillWidth: true }
                            Text { text: mail.drafts.length || ""; color: window.muted; font.pixelSize: 11; Layout.rightMargin: 10 }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Text {
                        text: mail.demo ? qsTr("A little room for your mail.") : qsTr("Your mail, thoughtfully simple.")
                        color: window.muted; font.pixelSize: 11; wrapMode: Text.Wrap
                        Layout.fillWidth: true; Layout.leftMargin: 9; Layout.bottomMargin: 12
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: window.rule }
                    RowLayout {
                        Layout.fillWidth: true; Layout.topMargin: 8
                        Rectangle {
                            width: 30; height: 30; radius: 15
                            color: AppTheme.dark ? "#414147" : "#dedee5"
                            Text { anchors.centerIn: parent; text: window.value(mail.account, "name").slice(0, 1).toUpperCase() || "M"; color: window.ink; font.weight: Font.DemiBold }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Text { text: window.value(mail.account, "name") || qsTr("Your account"); color: window.ink; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 12 }
                            Text { text: mail.demo ? qsTr("Example account") : window.value(mail.account, "address") || qsTr("Not connected"); color: window.muted; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 10 }
                        }
                        MailAction { iconOnly: true; iconName: "settings"; accessibleText: qsTr("Account settings"); onClicked: accountDialog.editAccount() }
                    }
                }
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: window.rule }
            }

            Rectangle {
                id: messagePane
                visible: !window.narrow || !window.readerVisible
                Layout.preferredWidth: 320
                Layout.minimumWidth: 250
                Layout.fillWidth: window.narrow
                Layout.fillHeight: true
                color: AppTheme.contentBackground
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    RowLayout {
                        Layout.fillWidth: true; Layout.preferredHeight: 53
                        Layout.leftMargin: 16; Layout.rightMargin: 14; spacing: 8
                        MailAction {
                            visible: window.width < 1080; iconOnly: true; iconName: "panel"; accessibleText: qsTr("Show mailboxes")
                            onClicked: window.sidebarExpanded = !window.sidebarExpanded
                        }
                        Text { text: window.folderTitle(); color: window.ink; font.pixelSize: 19; font.weight: Font.Bold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                        MailAction {
                            iconOnly: true; iconName: "refreshRounded"; accessibleText: qsTr("Check for mail")
                            enabled: !mail.busy && !window.draftsVisible && (mail.configured || mail.demo)
                            onClicked: mail.refresh()
                        }
                        MailAction {
                            visible: !window.sidebarVisible; iconOnly: true; iconName: "editRounded"; accessibleText: qsTr("New message")
                            enabled: mail.configured || mail.demo
                            onClicked: window.compose("new")
                        }
                    }
                    KosTextField {
                        id: searchField
                        objectName: "searchField"
                        Layout.fillWidth: true; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.bottomMargin: 12
                        implicitHeight: 30
                        placeholderText: qsTr("Search cached mail")
                        text: mail.search
                        visible: !window.draftsVisible
                        onTextEdited: searchDelay.restart()
                        Timer { id: searchDelay; interval: 200; onTriggered: mail.search = searchField.text }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: window.rule }
                    ListView {
                        id: messageList
                        objectName: "messageList"
                        visible: !window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; boundsBehavior: Flickable.StopAtBounds
                        model: mail.messages
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: AbstractButton {
                            id: messageRow
                            required property string uid
                            required property string uidValidity
                            required property string sender
                            required property string senderAddress
                            required property string subject
                            required property var date
                            required property string preview
                            required property bool unread
                            required property bool flagged
                            required property bool hasAttachments
                            width: ListView.view.width; height: 112
                            hoverEnabled: true
                            readonly property bool selected: mail.selectedUid === uid
                            Accessible.name: sender + ". " + subject + ". " + preview
                            onClicked: { mail.openMessage(uid, uidValidity); window.readerVisible = true }
                            background: Rectangle {
                                color: messageRow.selected ? (AppTheme.dark ? "#273f5a" : "#e5effb") : messageRow.hovered ? (AppTheme.dark ? "#27272b" : "#f7f7f9") : "transparent"
                                Rectangle { height: 1; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.leftMargin: 19; anchors.right: parent.right; color: window.rule }
                            }
                            contentItem: Item {
                                Rectangle { x: 9; y: 23; width: 6; height: 6; radius: 3; color: AppTheme.accent; visible: messageRow.unread }
                                ColumnLayout {
                                    anchors.fill: parent; anchors.leftMargin: 23; anchors.rightMargin: 17; anchors.topMargin: 14; anchors.bottomMargin: 13
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: messageRow.sender || messageRow.senderAddress; color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                        Text { text: window.dateLabel(messageRow.date); color: window.muted; font.pixelSize: 9 }
                                    }
                                    Text { text: messageRow.subject || qsTr("(No subject)"); color: window.ink; font.pixelSize: 12; font.weight: messageRow.unread ? Font.DemiBold : Font.Normal; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                    Text { text: messageRow.preview; color: window.muted; font.pixelSize: 11; lineHeight: 1.25; Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                                }
                            }
                        }
                        footer: Item {
                            width: messageList.width; height: mail.hasMore ? 48 : 0
                            MailAction { anchors.centerIn: parent; visible: mail.hasMore; text: qsTr("Load older messages"); enabled: !mail.busy; onClicked: mail.loadMore() }
                        }
                        Text {
                            anchors.centerIn: parent; width: parent.width - 40
                            visible: messageList.count === 0
                            text: mail.busy ? qsTr("Getting your mail…") : mail.search ? qsTr("No cached matches") : mail.configured || mail.demo ? qsTr("No messages here") : qsTr("Welcome to your new inbox.\nAdd an account to get started.")
                            color: window.muted; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; lineHeight: 1.5
                        }
                    }
                    ListView {
                        visible: window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        model: mail.drafts; clip: true
                        delegate: AbstractButton {
                            id: draftRow
                            required property var modelData
                            width: ListView.view.width; height: 86
                            Accessible.name: window.value(modelData, "subject") || qsTr("Untitled draft")
                            onClicked: composer.editDraft(mail.loadDraft(modelData.id))
                            background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; height: 1; width: parent.width; color: window.rule } }
                            contentItem: ColumnLayout {
                                spacing: 5
                                Text { text: window.value(draftRow.modelData, "subject") || qsTr("Untitled draft"); color: window.ink; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                Text { text: window.value(draftRow.modelData, "to") || qsTr("No recipient"); color: window.muted; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                Text { text: window.value(draftRow.modelData, "state"); color: window.muted; font.pixelSize: 10 }
                            }
                            leftPadding: 22; rightPadding: 18; topPadding: 14; bottomPadding: 14
                        }
                        Text { anchors.centerIn: parent; visible: mail.drafts.length === 0; text: qsTr("Your drafts will be saved here."); color: window.muted; font.pixelSize: 12 }
                    }
                }
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: window.rule }
            }

            Rectangle {
                id: reader
                objectName: "messageReader"
                visible: !window.narrow || window.readerVisible
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0
                color: AppTheme.contentBackground
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    RowLayout {
                        Layout.fillWidth: true; Layout.preferredHeight: 53
                        Layout.leftMargin: 21; Layout.rightMargin: 22; spacing: 12
                        MailAction { visible: window.narrow; iconOnly: true; iconName: "back"; accessibleText: qsTr("Back to messages"); onClicked: window.readerVisible = false }
                        MailAction { iconOnly: true; iconName: "undo"; accessibleText: qsTr("Reply"); enabled: window.hasMessage && !mail.sending; onClicked: window.compose("reply") }
                        MailAction { iconOnly: true; iconName: "forward"; accessibleText: qsTr("Forward"); enabled: window.hasMessage && !mail.sending; onClicked: window.compose("forward") }
                        Item { Layout.fillWidth: true }
                        MailAction { id: detailsButton; iconOnly: true; iconName: "reminder"; accessibleText: qsTr("Message details"); enabled: window.hasMessage; onClicked: details.open() }
                        MailAction { id: moreButton; iconOnly: true; iconName: "more"; accessibleText: qsTr("More message actions"); enabled: window.hasMessage; onClicked: moreMenu.openAtAnchor() }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: window.rule }
                    ScrollView {
                        visible: window.hasMessage && !window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentWidth: availableWidth; clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            width: parent.width
                            spacing: 0
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.margins: window.width < 1100 ? 24 : 36
                                spacing: 14
                                Text {
                                    text: window.value(window.message, "subject") || qsTr("(No subject)")
                                    color: window.ink; font.pixelSize: 25; font.weight: Font.Bold
                                    wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText
                                }
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 12
                                    Rectangle {
                                        width: 36; height: 36; radius: 18; color: AppTheme.dark ? "#3b424e" : "#e9edf3"
                                        Text { anchors.centerIn: parent; text: (window.value(window.message, "sender") || "?").slice(0, 1).toUpperCase(); color: AppTheme.accent; font.pixelSize: 16; font.weight: Font.DemiBold }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Text { text: window.value(window.message, "sender") || window.value(window.message, "senderAddress"); color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                        Text { text: qsTr("To: ") + window.value(window.message, "to"); color: window.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                    }
                                }
                                Text { text: window.dateLabel(window.message.date); color: window.muted; font.pixelSize: 11; Layout.fillWidth: true }
                                Rectangle { Layout.fillWidth: true; height: 1; color: window.rule; Layout.topMargin: 4; Layout.bottomMargin: 6 }
                                // Never interpret message HTML, links, images or remote resources.
                                TextEdit {
                                    id: messageBody
                                    objectName: "messageBody"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(200, contentHeight)
                                    text: window.value(window.message, "text")
                                    textFormat: TextEdit.PlainText
                                    readOnly: true; selectByMouse: true
                                    wrapMode: TextEdit.Wrap
                                    color: window.ink
                                    selectionColor: AppTheme.accent; selectedTextColor: "white"
                                    font.pixelSize: 14
                                }
                                Repeater {
                                    model: window.message.attachments || []
                                    delegate: Rectangle {
                                        id: attachmentRow
                                        required property var modelData
                                        required property int index
                                        Layout.fillWidth: true; implicitHeight: 48
                                        radius: 6; color: AppTheme.contentSecondaryBackground
                                        RowLayout {
                                            anchors.fill: parent; anchors.margins: 10
                                            VectorIcon { name: "album"; size: 18; color: window.muted }
                                            Text { text: attachmentRow.modelData.name; color: window.ink; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                            MailAction { iconOnly: true; iconName: "download"; accessibleText: qsTr("Save attachment"); onClicked: { saveAttachmentDialog.attachmentIndex = attachmentRow.index; saveAttachmentDialog.open() } }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Item {
                        visible: !window.hasMessage || window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Column {
                            anchors.centerIn: parent; spacing: 12; width: Math.min(310, parent.width - 48)
                            Text { width: parent.width; text: window.draftsVisible ? qsTr("Pick up where you left off.") : qsTr("A moment of quiet."); color: window.muted; font.pixelSize: 22; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                            Text { width: parent.width; text: window.draftsVisible ? qsTr("Choose a draft to continue writing.") : qsTr("Select a message to read it here."); color: window.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                            MailAction { anchors.horizontalCenter: parent.horizontalCenter; visible: !mail.configured && !mail.demo; text: qsTr("Add an account"); onClicked: accountDialog.editAccount() }
                        }
                    }
                }
            }
        }
        Rectangle {
            id: statusStrip
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: mail.error ? 44 : 29
            color: mail.error ? (AppTheme.dark ? "#452f30" : "#fff1ef") : window.sidebarColor
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: window.rule }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 14; spacing: 10
                Rectangle { width: 5; height: 5; radius: 3; color: mail.error ? AppTheme.destructive : mail.busy ? AppTheme.accent : window.muted }
                Text {
                    text: mail.error || (mail.demo ? qsTr("Demo · synthetic mail only · sending disabled") : mail.status || qsTr("Ready"))
                    color: mail.error ? AppTheme.destructive : window.muted; font.pixelSize: 10
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
                MailAction { visible: !!mail.error; implicitHeight: 24; text: qsTr("Dismiss"); onClicked: mail.clearError() }
                MailAction { implicitHeight: 24; iconOnly: true; iconName: "theme"; accessibleText: qsTr("Switch light / dark appearance"); onClicked: AppTheme.appearanceMode = AppTheme.dark ? "light" : "dark" }
            }
        }
    }

    KosWindowResizeHandles { targetWindow: window }

    LiquidPopover {
        id: moreMenu
        sourceItem: mailScene
        anchorItem: moreButton
        LiquidPopoverItem { text: qsTr("Reply"); iconName: "undo"; enabled: !mail.sending; onTriggered: window.compose("reply") }
        LiquidPopoverItem { text: qsTr("Reply to all"); enabled: !mail.sending; onTriggered: window.compose("replyAll") }
        LiquidPopoverItem { text: qsTr("Forward"); iconName: "forward"; enabled: !mail.sending; onTriggered: window.compose("forward") }
        LiquidPopoverItem { text: qsTr("Message details"); iconName: "reminder"; onTriggered: details.open() }
    }
    LiquidContentPopover {
        id: details
        objectName: "messageDetailsPopover"
        sourceItem: mailScene
        anchorItem: detailsButton
        preferredSide: "bottom"
        width: Math.min(410, window.width - 32)
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: qsTr("Message details"); color: window.ink; font.pixelSize: 16; font.weight: Font.DemiBold }
            Repeater {
                model: [ {title: qsTr("From"), value: window.value(window.message, "senderAddress")},
                    {title: qsTr("To"), value: window.value(window.message, "to")},
                    {title: qsTr("Cc"), value: window.value(window.message, "cc")},
                    {title: qsTr("Date"), value: window.dateLabel(window.message.date)},
                    {title: qsTr("Message ID"), value: window.value(window.message, "messageId")} ]
                delegate: ColumnLayout {
                    required property var modelData
                    visible: modelData.value.length > 0
                    Layout.fillWidth: true; spacing: 3
                    Text { text: parent.modelData.title; color: window.muted; font.pixelSize: 10 }
                    TextEdit { text: parent.modelData.value; color: window.ink; font.pixelSize: 12; readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true }
                }
            }
            Text { text: qsTr("Read-only mailbox · no server flags are changed."); color: window.muted; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
    }
    MailComposer { id: composer; mail: window.mail; sourceItem: mailScene; hostWindow: window }
    AccountDialog { id: accountDialog; mail: window.mail; sourceItem: mailScene; hostWindow: window }
    FileDialog {
        id: saveAttachmentDialog
        property int attachmentIndex: -1
        title: qsTr("Save attachment")
        fileMode: FileDialog.SaveFile
        onAccepted: mail.saveAttachment(attachmentIndex, selectedFile.toString())
    }
    Shortcut { sequence: "Ctrl+N"; enabled: !composer.visible && !accountDialog.visible && (mail.configured || mail.demo); onActivated: window.compose("new") }
    Shortcut { sequence: "Ctrl+F"; enabled: !composer.visible && !accountDialog.visible; onActivated: { window.readerVisible = false; searchField.forceActiveFocus() } }
    Shortcut { sequence: "Ctrl+R"; enabled: !composer.visible && !accountDialog.visible && !mail.busy; onActivated: mail.refresh() }
}

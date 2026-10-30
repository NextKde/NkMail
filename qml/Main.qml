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
    color: "transparent"
    background: Item {}
    font.family: "sans-serif"
    font.pixelSize: AppTheme.compactControlFontSize

    readonly property bool narrow: width < 800
    readonly property bool liquidControls: AppTheme.materialMode !== "solid"
        && !AppTheme.reduceTransparency
        && GraphicsInfo.api !== GraphicsInfo.Software
    property bool sidebarExpanded: false
    property bool readerVisible: false
    property bool draftsVisible: false
    // Preferred widths survive temporary narrow layouts; actual widths always
    // leave space for the reader and never squeeze the list below its minimum.
    property int sidebarPreferredWidth: 236
    property int listPreferredWidth: 320
    readonly property int sidebarWidth: Math.max(190, Math.min(340, sidebarPreferredWidth,
        width - (narrow ? 260 : 560)))
    readonly property int listWidth: Math.max(260, Math.min(520, listPreferredWidth,
        width - (sidebarVisible ? sidebarWidth : 0) - 300))
    readonly property bool sidebarVisible: width >= 1080 || sidebarExpanded
    readonly property var message: mail.selectedMessage || ({})
    readonly property color selectedSurface: AppTheme.withAlpha(AppTheme.accent, AppTheme.dark ? 0.18 : 0.10)
    readonly property color hoverSurface: AppTheme.cardHover
    readonly property bool hasMessage: String(mail.selectedUid || "").length > 0
    readonly property color ink: AppTheme.text
    readonly property color muted: AppTheme.mutedText
    readonly property color rule: AppTheme.border
    readonly property color sidebarColor: AppTheme.sidebar

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
        return AppTheme.formatDateTime(d, Locale.ShortFormat)
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
        else if (scenario === "list") readerVisible = false
        else if (scenario === "reader") readerVisible = hasMessage
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

        // Calendar's shell contract: the window is transparent, while these
        // two flush planes own the actual sidebar/content surfaces. Controls
        // are never included in the capture source.
        Item {
            id: chromePlanes
            objectName: "chromePlanes"
            anchors.fill: parent
            z: 0

            Rectangle {
                id: sidebarPlane
                objectName: "sidebarPlane"
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: window.sidebarVisible ? window.sidebarWidth : 0
                color: window.sidebarColor
                opacity: window.liquidControls ? 0.95 : 1
            }

            Rectangle {
                id: contentPlane
                objectName: "contentPlane"
                anchors.left: sidebarPlane.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: AppTheme.contentBackground

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: sidebarPlane.width > 0 ? 1 : 0
                    color: window.rule
                }
            }
        }

        LiquidBackdrop {
            id: chromeBackdrop
            objectName: "chromeBackdrop"
            sourceItem: chromePlanes
            registerAsDefault: false
            active: window.liquidControls
        }

        // The header owns every top-level action; none of the body panes has
        // its own toolbar. Interactive children sit above the drag background.
        Item {
            id: floatingHeader
            objectName: "floatingHeader"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: MailStyle.shellHeaderHeight
            z: 3

            WindowChrome {
                id: windowChrome
                anchors.fill: parent
                targetWindow: window
            }

            Item {
                id: sidebarHeader
                objectName: "sidebarHeader"
                visible: window.sidebarVisible
                width: sidebarPlane.width
                height: parent.height
                Text {
                    x: 14; y: 4
                    width: parent.width - 28; height: 26
                    text: qsTr("Mail"); font.pixelSize: 24; font.weight: Font.Bold
                    color: window.ink; elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
                MailAction {
                    anchors.right: parent.right; anchors.rightMargin: 14
                    y: 36
                    iconOnly: true; iconName: "editRounded"; accessibleText: qsTr("New message")
                    floatingAction: true; shellBackdrop: chromeBackdrop
                    objectName: "sidebarComposeButton"
                    enabled: mail.configured || mail.demo
                    onClicked: window.compose("new")
                }
            }

            Item {
                id: listHeader
                objectName: "listHeader"
                visible: messagePane.visible
                x: contentScene.x + messagePane.x
                width: messagePane.width
                height: parent.height
                readonly property real controlsInset: window.narrow ? windowChrome.controlsReservedWidth : 0
                RowLayout {
                    x: 14; y: 4
                    width: Math.max(0, parent.width - 28 - listHeader.controlsInset)
                    height: 26; spacing: 8
                    MailAction {
                        objectName: "sidebarToggleButton"
                        visible: window.width < 1080; iconOnly: true; iconName: "panel"; accessibleText: qsTr("Show mailboxes")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        onClicked: window.sidebarExpanded = !window.sidebarExpanded
                    }
                    Text {
                        text: window.folderTitle(); color: window.ink; font.pixelSize: 19; font.weight: Font.Bold
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        elide: Text.ElideRight; textFormat: Text.PlainText
                    }
                    MailAction {
                        objectName: "refreshButton"
                        iconOnly: true; iconName: "refreshRounded"; accessibleText: qsTr("Check for mail")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        enabled: !mail.busy && !window.draftsVisible && (mail.configured || mail.demo)
                        onClicked: mail.refresh()
                    }
                    MailAction {
                        objectName: "headerComposeButton"
                        visible: !window.sidebarVisible; iconOnly: true; iconName: "editRounded"; accessibleText: qsTr("New message")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        enabled: mail.configured || mail.demo
                        onClicked: window.compose("new")
                    }
                }
                KosTextField {
                    id: searchField
                    objectName: "searchField"
                    x: 14; y: 34; width: parent.width - 28; height: 30
                    implicitHeight: 30
                    placeholderText: qsTr("Search cached mail")
                    text: mail.search
                    visible: !window.draftsVisible
                    onTextEdited: searchDelay.restart()
                    Timer { id: searchDelay; interval: 200; onTriggered: mail.search = searchField.text }
                }
            }

            Item {
                id: readerHeader
                objectName: "readerHeader"
                visible: reader.visible
                x: contentScene.x + reader.x
                width: reader.width
                height: parent.height
                RowLayout {
                    x: 21; y: 36; width: parent.width - 43; height: 26; spacing: 12
                    MailAction {
                        objectName: "readerBackButton"
                        visible: window.narrow; iconOnly: true; iconName: "back"; accessibleText: qsTr("Back to messages")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        onClicked: window.readerVisible = false
                    }
                    MailAction {
                        objectName: "replyButton"
                        iconOnly: true; iconName: "undo"; accessibleText: qsTr("Reply")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        enabled: window.hasMessage && !mail.sending
                        onClicked: window.compose("reply")
                    }
                    MailAction {
                        objectName: "forwardButton"
                        visible: readerHeader.width >= 360
                        iconOnly: true; iconName: "forward"; accessibleText: qsTr("Forward")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        enabled: window.hasMessage && !mail.sending
                        onClicked: window.compose("forward")
                    }
                    Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                    MailAction {
                        id: detailsButton
                        objectName: "detailsButton"
                        visible: readerHeader.width >= 360
                        iconOnly: true; iconName: "reminder"; accessibleText: qsTr("Message details")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        enabled: window.hasMessage; onClicked: details.open()
                    }
                    MailAction {
                        id: moreButton
                        objectName: "moreButton"
                        iconOnly: true; iconName: "more"; accessibleText: qsTr("More message actions")
                        floatingAction: true; shellBackdrop: chromeBackdrop
                        enabled: window.hasMessage; onClicked: moreMenu.openAtAnchor()
                    }
                }
            }
        }

        Item {
                id: sidebar
                objectName: "sidebarBody"
                visible: window.sidebarVisible
                anchors.left: parent.left
                anchors.top: floatingHeader.bottom
                anchors.bottom: statusStrip.top
                width: sidebarPlane.width
                z: 1
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14
                    spacing: 4
                    Text {
                        text: qsTr("MAILBOXES"); color: window.muted; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1
                        Layout.leftMargin: 9; Layout.topMargin: 18; Layout.bottomMargin: 8
                    }
                    Repeater {
                        model: mail.folders
                        delegate: AbstractButton {
                            id: folderButton
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: MailStyle.sidebarRowHeight
                            hoverEnabled: true
                            Accessible.name: modelData.title
                            onClicked: {
                                window.draftsVisible = false; window.readerVisible = false
                                mail.selectFolder(modelData.id)
                                if (window.width < 1080) window.sidebarExpanded = false
                            }
                            background: Rectangle {
                                radius: AppTheme.smallRadius
                                color: !window.draftsVisible && mail.currentFolder === folderButton.modelData.id
                                    ? window.selectedSurface
                                    : folderButton.hovered ? window.hoverSurface : "transparent"
                                border.width: folderButton.activeFocus ? 1 : 0
                                border.color: AppTheme.focusRing
                            }
                            contentItem: RowLayout {
                                spacing: 10
                                VectorIcon { name: "folder"; size: 17; color: (!window.draftsVisible && mail.currentFolder === folderButton.modelData.id) ? AppTheme.accent : AppTheme.mutedText; Layout.leftMargin: 9 }
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
                        Layout.fillWidth: true; implicitHeight: MailStyle.sidebarRowHeight
                        hoverEnabled: true
                        Accessible.name: qsTr("Local drafts")
                        onClicked: { window.draftsVisible = true; window.readerVisible = false; if (window.width < 1080) window.sidebarExpanded = false }
                        background: Rectangle {
                            radius: AppTheme.smallRadius
                            color: window.draftsVisible ? window.selectedSurface : draftFolder.hovered ? window.hoverSurface : "transparent"
                            border.width: draftFolder.activeFocus ? 1 : 0
                            border.color: AppTheme.focusRing
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            VectorIcon { name: "editRounded"; size: 17; color: window.draftsVisible ? AppTheme.accent : AppTheme.mutedText; Layout.leftMargin: 9 }
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
                        MailAction { iconOnly: true; iconName: "settings"; accessibleText: qsTr("Account settings"); floatingAction: true; shellBackdrop: chromeBackdrop; onClicked: accountDialog.editAccount() }
                    }
                }
        }

        RowLayout {
            id: contentScene
            objectName: "contentScene"
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: floatingHeader.bottom
            anchors.bottom: statusStrip.top
            spacing: 0
            z: 1

            Rectangle {
                id: messagePane
                objectName: "messagePane"
                visible: !window.narrow || !window.readerVisible
                Layout.preferredWidth: window.listWidth
                Layout.minimumWidth: window.narrow ? 260 : window.listWidth
                Layout.maximumWidth: window.narrow ? -1 : window.listWidth
                Layout.fillWidth: window.narrow
                Layout.fillHeight: true
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
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
                            width: ListView.view.width; height: MailStyle.messageRowHeight
                            hoverEnabled: true
                            readonly property bool selected: mail.selectedUid === uid
                            Accessible.name: sender + ". " + subject + ". " + preview
                            onClicked: { mail.openMessage(uid, uidValidity); window.readerVisible = true }
                            background: Rectangle {
                                color: messageRow.selected ? window.selectedSurface : messageRow.hovered ? window.hoverSurface : "transparent"
                                border.width: messageRow.activeFocus ? 1 : 0
                                border.color: AppTheme.focusRing
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
                                        Text { text: window.dateLabel(messageRow.date); color: window.muted; font.pixelSize: AppTheme.compactControlFontSize }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text { text: messageRow.subject || qsTr("(No subject)"); color: window.ink; font.pixelSize: 12; font.weight: messageRow.unread ? Font.DemiBold : Font.Normal; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                        Text { text: messageRow.flagged ? "★" : ""; color: AppTheme.accent; font.pixelSize: 12; visible: messageRow.flagged; Accessible.name: qsTr("Flagged") }
                                        VectorIcon { name: "album"; size: 14; color: window.muted; visible: messageRow.hasAttachments; Accessible.name: qsTr("Has attachment") }
                                    }
                                    Text { text: messageRow.preview; color: window.muted; font.pixelSize: 11; lineHeight: 1.25; Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                                }
                            }
                        }
                        footer: Item {
                            width: messageList.width; height: mail.hasMore ? 48 : 0
                            MailAction { anchors.centerIn: parent; visible: mail.hasMore; text: qsTr("Load older messages"); enabled: !mail.busy; onClicked: mail.loadMore() }
                        }
                        KosEmptyState {
                            anchors.centerIn: parent
                            width: parent.width - 40
                            visible: messageList.count === 0
                            symbol: mail.busy ? "…" : mail.search ? "⌕" : "✉"
                            title: mail.busy ? qsTr("Getting your mail…") : mail.search ? qsTr("No cached matches") : mail.configured || mail.demo ? qsTr("No messages here") : qsTr("Welcome to your new inbox")
                            description: mail.busy ? "" : mail.search ? qsTr("Try a different search term.") : mail.configured || mail.demo ? qsTr("There is nothing to show in this mailbox yet.") : qsTr("Add an account to start reading mail locally.")
                            actionText: !mail.configured && !mail.demo ? qsTr("Add an account") : ""
                            onActionTriggered: accountDialog.editAccount()
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
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent; spacing: 0
                    ScrollView {
                        id: readerScroll
                        objectName: "readerScroll"
                        visible: window.hasMessage && !window.draftsVisible
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentWidth: availableWidth; clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            width: readerScroll.availableWidth
                            spacing: 0
                            ColumnLayout {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: false
                                Layout.preferredWidth: Math.max(0, Math.min(MailStyle.readerMaximumWidth, readerScroll.availableWidth - (window.width < 1100 ? 48 : 72)))
                                Layout.maximumWidth: Math.max(0, Math.min(MailStyle.readerMaximumWidth, readerScroll.availableWidth - (window.width < 1100 ? 48 : 72)))
                                Layout.minimumWidth: 0
                                spacing: 14
                                Text {
                                    text: window.value(window.message, "subject") || qsTr("(No subject)")
                                    color: window.ink; font.pixelSize: 25; font.weight: Font.Bold
                                    wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText
                                }
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 12
                                    Rectangle {
                                        width: 36; height: 36; radius: 18; color: MailStyle.avatar
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
        // Generous invisible hit targets keep the one-pixel column rules quiet.
        // Focused dividers are also adjustable with the arrow keys.
        component ColumnDivider: MouseArea {
            id: divider
            required property bool sidebarHandle
            property real pressSceneX: 0
            property int startWidth: 0
            width: 8
            y: floatingHeader.height
            height: statusStrip.y - y
            z: 4
            cursorShape: Qt.SplitHCursor
            hoverEnabled: true
            activeFocusOnTab: true
            preventStealing: true
            Accessible.role: Accessible.Slider
            Accessible.name: sidebarHandle ? qsTr("Mailbox column width") : qsTr("Message column width")
            Accessible.description: qsTr("Drag or use Left and Right to resize. Double-click to reset.")
            function resizeTo(value) {
                if (sidebarHandle) window.sidebarPreferredWidth = Math.round(Math.max(190, Math.min(340, value, window.width - 560)))
                else window.listPreferredWidth = Math.round(Math.max(260, Math.min(520, value,
                    window.width - (window.sidebarVisible ? window.sidebarWidth : 0) - 300)))
            }
            onPressed: mouse => {
                pressSceneX = mapToItem(mailScene, mouse.x, mouse.y).x
                startWidth = sidebarHandle ? window.sidebarWidth : window.listWidth
                forceActiveFocus()
            }
            onPositionChanged: mouse => {
                if (pressed) resizeTo(startWidth + mapToItem(mailScene, mouse.x, mouse.y).x - pressSceneX)
            }
            onDoubleClicked: {
                if (sidebarHandle) window.sidebarPreferredWidth = 236
                else window.listPreferredWidth = 320
            }
            Keys.onLeftPressed: resizeTo((sidebarHandle ? window.sidebarWidth : window.listWidth) - 10)
            Keys.onRightPressed: resizeTo((sidebarHandle ? window.sidebarWidth : window.listWidth) + 10)
            Rectangle {
                anchors.centerIn: parent; width: 2; height: parent.height
                color: AppTheme.accent
                opacity: divider.pressed || divider.activeFocus ? 0.8 : divider.containsMouse ? 0.35 : 0
            }
        }
        ColumnDivider {
            objectName: "sidebarDivider"
            sidebarHandle: true
            visible: window.sidebarVisible && !window.narrow
            x: sidebar.width - width / 2
        }
        ColumnDivider {
            objectName: "messageDivider"
            sidebarHandle: false
            visible: !window.narrow
            x: contentScene.x + messagePane.x + messagePane.width - width / 2
        }

        Rectangle {
            id: statusStrip
            objectName: "statusStrip"
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
        sourceItem: chromePlanes
        anchorItem: moreButton
        LiquidPopoverItem { text: qsTr("Reply"); iconName: "undo"; enabled: !mail.sending; onTriggered: window.compose("reply") }
        LiquidPopoverItem { text: qsTr("Reply to all"); enabled: !mail.sending; onTriggered: window.compose("replyAll") }
        LiquidPopoverItem { text: qsTr("Forward"); iconName: "forward"; enabled: !mail.sending; onTriggered: window.compose("forward") }
        LiquidPopoverItem { text: qsTr("Message details"); iconName: "reminder"; onTriggered: details.open() }
    }
    LiquidContentPopover {
        id: details
        objectName: "messageDetailsPopover"
        sourceItem: chromePlanes
        anchorItem: detailsButton.visible ? detailsButton : moreButton
        preferredSide: "bottom"
        width: Math.min(360, window.width - 16)
        implicitHeight: Math.min(360, detailsColumn.implicitHeight + topPadding + bottomPadding)
        height: implicitHeight
        contentItem: ScrollView {
            id: detailsScroll
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ColumnLayout {
                id: detailsColumn
                width: detailsScroll.availableWidth
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Message details"); color: window.ink; font.pixelSize: 15; font.weight: Font.DemiBold; Layout.fillWidth: true }
                    MailAction { iconOnly: true; iconName: "close"; accessibleText: qsTr("Close message details"); onClicked: details.close() }
                }
                Repeater {
                    model: [ {title: qsTr("From"), value: window.value(window.message, "senderAddress")},
                        {title: qsTr("To"), value: window.value(window.message, "to")},
                        {title: qsTr("Cc"), value: window.value(window.message, "cc")},
                        {title: qsTr("Date"), value: window.dateLabel(window.message.date)},
                        {title: qsTr("Message ID"), value: window.value(window.message, "messageId")} ]
                    delegate: ColumnLayout {
                        id: detailsEntry
                        required property var modelData
                        visible: modelData.value.length > 0
                        Layout.fillWidth: true; spacing: 3
                        Text { text: detailsEntry.modelData.title; color: window.muted; font.pixelSize: 11 }
                        TextEdit { text: detailsEntry.modelData.value; color: window.ink; font.pixelSize: 13; readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true }
                    }
                }
                Text { text: qsTr("Read-only mailbox · no server flags are changed."); color: window.muted; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
    }
    MailComposer { id: composer; mail: window.mail; sourceItem: chromePlanes; hostWindow: window }
    AccountDialog { id: accountDialog; mail: window.mail; sourceItem: chromePlanes; hostWindow: window }
    FileDialog {
        id: saveAttachmentDialog
        property int attachmentIndex: -1
        title: qsTr("Save attachment")
        fileMode: FileDialog.SaveFile
        onAccepted: mail.saveAttachment(attachmentIndex, selectedFile.toString())
    }
    Shortcut { sequence: "Ctrl+N"; enabled: !composer.visible && !accountDialog.visible && (mail.configured || mail.demo); onActivated: window.compose("new") }
    Shortcut { sequence: "Ctrl+F"; enabled: !composer.visible && !accountDialog.visible; onActivated: { window.draftsVisible = false; window.readerVisible = false; searchField.forceActiveFocus() } }
    Shortcut {
        sequence: "Escape"
        enabled: !composer.visible && !accountDialog.visible && !details.visible && !moreMenu.visible
        onActivated: {
            if (window.sidebarExpanded) window.sidebarExpanded = false
            else if (window.narrow && window.readerVisible) window.readerVisible = false
            else if (searchField.activeFocus && searchField.text.length) { searchField.text = ""; mail.search = "" }
        }
    }
    Shortcut { sequence: "Ctrl+R"; enabled: !composer.visible && !accountDialog.visible && !mail.busy; onActivated: mail.refresh() }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

Rectangle {
    id: root
    required property Item surface
    property string localeName: Qt.locale().name
    required property var mail
    required property QtObject selection
    property bool draftsVisible: false
    property bool groupThreads: true
    property bool showSummary: true
    property bool showAvatars: false
    readonly property color ink: AppTheme.text
    readonly property color muted: AppTheme.mutedText
    readonly property color rule: AppTheme.border
    readonly property color selectedSurface: AppTheme.withAlpha(AppTheme.accent, AppTheme.dark ? 0.18 : 0.10)
    readonly property color hoverSurface: AppTheme.cardHover
    signal messageOpened()
    signal composeRequested(string mode)
    signal addAccountRequested()
    signal editDraftRequested(string draftId)

    function showReviewMenu() {
        if (!mail.demo) return
        const row = messageList.itemAtIndex(0)
        if (row) row.openContextMenu()
    }
    objectName: "messagePane"
    color: "transparent"
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        ListView {
            id: messageList
            objectName: "messageList"
            visible: !root.draftsVisible
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; boundsBehavior: Flickable.StopAtBounds
            model: mail.messages
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: AbstractButton {
                id: messageRow
                required property int index
                required property string uid
                required property string folder
                required property string uidValidity
                required property string sender
                required property string senderAddress
                required property string subject
                required property var date
                required property string preview
                required property bool unread
                required property bool flagged
                required property bool hasAttachments
                required property string threadKey
                required property int threadCount
                required property bool threadFirst
                readonly property bool hiddenByThread: root.groupThreads && !threadFirst && root.selection.collapsedThreads[threadKey] === true
                width: ListView.view.width
                visible: !hiddenByThread
                height: visible ? MailStyle.messageRowHeight : 0
                implicitHeight: visible ? MailStyle.messageRowHeight : 0
                hoverEnabled: true
                readonly property bool selected: mail.selectedUid === uid && (mail.selectedMessage.folder || (mail.smartMailbox ? "" : mail.currentFolder)) === folder
                readonly property color rowInk: selected ? "#ffffff" : root.ink
                readonly property color rowMuted: selected ? "#ffffff" : root.muted
                readonly property color rowAccent: selected ? "#ffffff" : AppTheme.accent
                readonly property bool multiSelected: root.selection.isRowSelected(index)
                readonly property int messageIndex: (mail.messages && typeof mail.messages.indexOf === "function")
                    ? mail.messages.indexOf(folder, uidValidity, uid) : -1
                readonly property bool bulkMenu: root.selection.isRowSelected(index) && root.selection.selectionSize > 1
                readonly property bool canCompose: !bulkMenu && !mail.sending
                    && (mail.selectedMessage.folder || (mail.smartMailbox ? "" : mail.currentFolder)) === folder && mail.selectedMessage.uid === uid && mail.selectedMessage.uidValidity === uidValidity
                function composeMessage(mode) {
                    if (!canCompose) return
                    rowMenu.close()
                    root.composeRequested(mode)
                }
                function contextCommand(action, value) {
                    if (messageIndex < 0) return
                    const rows = bulkMenu ? root.selection.selectedIndexList() : [messageIndex]
                    if (action === "read") mail.setMessagesRead(rows, value)
                    else if (action === "flag") mail.setMessagesFlagged(rows, value)
                    else if (action === "archive") mail.archiveMessages(rows)
                    else if (action === "delete") mail.deleteMessages(rows)
                    else if (action === "move") mail.moveMessages(rows, value)
                }
                function openContextMenu() {
                    if (!root.selection.isRowSelected(index)) root.selection.clearSelection()
                    selectRow()
                    rowMenu.openAtAnchor()
                }
                function selectRow() { mail.openMessage(uid, uidValidity, folder); root.messageOpened() }
                function openRow() {
                    selectRow()
                    if (unread) mail.setMessageRead(messageIndex, true)
                }
                Accessible.name: sender + ". " + subject + (root.showSummary ? ". " + preview : "")
                onClicked: {}   // 鼠标点击由 TapHandler 处理；键盘见下方 Keys
                Keys.onReturnPressed: openRow()
                Keys.onEnterPressed: openRow()
                Keys.onSpacePressed: openRow()
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: function(point) {
                        if (point.modifiers & Qt.ShiftModifier) root.selection.selectRangeTo(index)
                        else if (point.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) root.selection.toggleRow(index)
                        else { root.selection.clearSelection(); messageRow.openRow() }
                    }
                }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: messageRow.openContextMenu()
                }
                LiquidPopover {
                    id: rowMenu
                    objectName: "messageRowMenu"
                    sourceItem: root.surface
                    anchorItem: messageRow
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                    LiquidPopoverItem { text: qsTr("打开"); iconName: "reminder"; enabled: !messageRow.bulkMenu; onTriggered: messageRow.openRow() }
                    LiquidPopoverItem { objectName: "rowReply"; text: qsTr("Reply"); iconName: "undo"; enabled: messageRow.canCompose; onTriggered: messageRow.composeMessage("reply") }
                    LiquidPopoverItem { objectName: "rowReplyAll"; text: qsTr("Reply to all"); iconName: "arrows"; enabled: messageRow.canCompose; onTriggered: messageRow.composeMessage("replyAll") }
                    LiquidPopoverItem { objectName: "rowForward"; text: qsTr("Forward"); iconName: "forward"; enabled: messageRow.canCompose; onTriggered: messageRow.composeMessage("forward") }
                    Rectangle { implicitHeight: 1; Layout.fillWidth: true; color: root.rule }
                    LiquidPopoverItem {
                        text: messageRow.unread ? qsTr("标为已读") : qsTr("标为未读")
                        iconName: messageRow.unread ? "envelopeOpen" : "envelope"
                        onTriggered: messageRow.contextCommand("read", messageRow.unread)
                    }
                    LiquidPopoverItem {
                        text: messageRow.flagged ? qsTr("移除旗标") : qsTr("添加旗标")
                        iconName: "flag"
                        onTriggered: messageRow.contextCommand("flag", !messageRow.flagged)
                    }
                    LiquidPopoverItem {
                        objectName: "moveToMenuItem"
                        text: qsTr("移动到…")
                        iconName: "folder"
                        enabled: mail.folders.filter(f => f.id !== messageRow.folder).length > 0
                        onTriggered: { rowMenu.close(); messageMoveMenu.openAtAnchor() }
                    }
                    LiquidPopoverItem { text: qsTr("归档"); iconName: "archive"; onTriggered: messageRow.contextCommand("archive") }
                    LiquidPopoverItem { text: qsTr("删除"); iconName: "trash"; onTriggered: messageRow.contextCommand("delete") }
                }
                LiquidPopover {
                    id: messageMoveMenu
                    objectName: "messageMoveMenu"
                    sourceItem: root.surface
                    anchorItem: messageRow
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                    Repeater {
                        model: mail.folders.filter(f => f.id !== messageRow.folder)
                        LiquidPopoverItem {
                            text: modelData.title || modelData.id
                            iconName: "folder"
                            onTriggered: {
                                messageMoveMenu.close()
                                messageRow.contextCommand("move", modelData.id)
                            }
                        }
                    }
                }
                background: Item {
                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: 8; anchors.rightMargin: 8
                        anchors.topMargin: 2; anchors.bottomMargin: 2
                        radius: 6
                        color: messageRow.selected ? AppTheme.accent : messageRow.multiSelected ? root.selectedSurface : messageRow.hovered ? root.hoverSurface : "transparent"
                        border.width: messageRow.activeFocus ? 1 : 0
                        border.color: AppTheme.focusRing
                    }
                    Rectangle {
                        height: 1
                        anchors.top: parent.top
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.leftMargin: 24; anchors.rightMargin: 24
                        color: root.rule
                        opacity: 0.5
                        visible: messageRow.index > 0 && !messageRow.selected
                    }
                }
                contentItem: Item {
                    Rectangle {
                        visible: root.showAvatars
                        x: 24; y: 12; width: 28; height: 28; radius: 14
                        color: messageRow.selected ? Qt.rgba(1, 1, 1, 0.22) : MailStyle.avatar
                        Text {
                            anchors.centerIn: parent
                            text: (messageRow.sender || messageRow.senderAddress || "?").trim().slice(0, 1).toUpperCase()
                            color: messageRow.selected ? "white" : AppTheme.accent
                            font.pixelSize: 13; font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                        }
                    }
                    Rectangle { x: 12; y: 14; width: 6; height: 6; radius: 3; color: messageRow.rowAccent; visible: messageRow.unread }
                    ColumnLayout {
                        anchors.fill: parent; anchors.leftMargin: root.showAvatars ? 60 : 24; anchors.rightMargin: 24; anchors.topMargin: 7; anchors.bottomMargin: 7
                        spacing: 3
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: messageRow.sender || messageRow.senderAddress; color: messageRow.rowInk; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                            Rectangle {
                                id: threadBadge
                                visible: root.groupThreads && messageRow.threadFirst && messageRow.threadCount > 1
                                implicitWidth: threadBadgeRow.implicitWidth + 12
                                implicitHeight: 18
                                radius: 9
                                color: messageRow.selected ? AppTheme.withAlpha("#ffffff", 0.12) : root.hoverSurface
                                border.width: 1
                                border.color: messageRow.selected ? AppTheme.withAlpha("#ffffff", 0.18) : root.rule
                                RowLayout {
                                    id: threadBadgeRow
                                    anchors.centerIn: parent
                                    spacing: 3
                                    Text { text: messageRow.threadCount; color: messageRow.rowMuted; font.pixelSize: 10 }
                                    VectorIcon { name: "chevronsUpDown"; size: 12; color: messageRow.rowMuted }
                                }
                                TapHandler {
                                    grabPermissions: PointerHandler.CanTakeOverFromAnything
                                    onTapped: root.selection.toggleThread(messageRow.threadKey)
                                }
                            }
                            Text { text: MailLabels.dateLabel(messageRow.date, root.localeName); color: messageRow.rowMuted; font.pixelSize: AppTheme.compactControlFontSize }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: messageRow.subject || qsTr("(No subject)"); color: messageRow.rowInk; font.pixelSize: 12; font.weight: messageRow.unread ? Font.DemiBold : Font.Normal; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                            Text { text: messageRow.flagged ? "★" : ""; color: messageRow.rowAccent; font.pixelSize: 12; visible: messageRow.flagged; Accessible.name: qsTr("Flagged") }
                            VectorIcon { name: "album"; size: 14; color: messageRow.rowMuted; visible: messageRow.hasAttachments; Accessible.name: qsTr("Has attachment") }
                        }
                        Text { objectName: "messagePreview"; visible: root.showSummary; text: messageRow.preview; color: messageRow.rowMuted; font.pixelSize: 11; lineHeight: 1.25; Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.NoWrap; maximumLineCount: 1; elide: Text.ElideRight; textFormat: Text.PlainText }
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
                objectName: "messageEmptyState"
                title: mail.busy ? qsTr("Getting your mail…") : mail.search ? qsTr("No cached matches") : mail.unreadOnly ? qsTr("No unread messages in cached mail") : mail.configured || mail.demo ? qsTr("No messages here") : qsTr("Welcome to your new inbox")
                description: mail.busy ? "" : mail.unreadOnly ? qsTr("Only cached unread messages are shown. Clear the filter to browse all messages.") : mail.search ? qsTr("Try a different search term.") : mail.configured || mail.demo ? qsTr("There is nothing to show in this mailbox yet.") : qsTr("Add an account to start reading mail locally.")
                actionText: mail.unreadOnly ? qsTr("Show all cached messages") : !mail.configured && !mail.demo ? qsTr("Add an account") : ""
                onActionTriggered: {
                    if (mail.unreadOnly) mail.unreadOnly = false
                    else root.addAccountRequested()
                }
            }
        }
        ListView {
            visible: root.draftsVisible
            Layout.fillWidth: true; Layout.fillHeight: true
            model: mail.drafts; clip: true
            delegate: AbstractButton {
                id: draftRow
                required property var modelData
                width: ListView.view.width; height: 86
                Accessible.name: MailLabels.value(modelData, "subject") || qsTr("Untitled draft")
                onClicked: root.editDraftRequested(modelData.id)
                background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; height: 1; width: parent.width; color: root.rule } }
                contentItem: ColumnLayout {
                    spacing: 5
                    Text { text: MailLabels.value(draftRow.modelData, "subject") || qsTr("Untitled draft"); color: root.ink; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Text { text: MailLabels.value(draftRow.modelData, "to") || qsTr("No recipient"); color: root.muted; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Text { text: MailLabels.draftStateLabel(draftRow.modelData.state); color: root.muted; font.pixelSize: 10 }
                }
                leftPadding: 22; rightPadding: 18; topPadding: 14; bottomPadding: 14
            }
            Text { anchors.centerIn: parent; visible: mail.drafts.length === 0; text: qsTr("Your drafts will be saved here."); color: root.muted; font.pixelSize: 12 }
        }
    }
}

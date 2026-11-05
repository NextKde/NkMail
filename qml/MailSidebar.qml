import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui
import "MailboxNavigation.js" as Navigation

Item {
        id: root

    required property var mail
    required property Item surface
    property var sidebarAccounts: []
    property bool draftsVisible: false
    property int unreadRevision: 0
    property string navigationOrigin: "favorite"
    readonly property color muted: AppTheme.mutedText
    property var collapsedAccounts: ({})
    signal editAccountRequested(var account)
    signal removeAccountRequested(string accountId, string name)
    signal addAccountRequested()
    signal mailboxOpened()
    signal draftsRequested()
    function toggleAccountCollapsed(id) {
        const next = Object.assign({}, collapsedAccounts)
        if (next[id]) delete next[id]; else next[id] = true
        collapsedAccounts = next
    }

    function accountAddress(accountId) {
        if (accountId === mail.currentAccountId) return String(mail.account.address || "")
        const account = (mail.accounts || []).find(a => a.id === accountId)
        return account ? String(account.address || "") : ""
    }
    function scopedTitle(title, accountId) {
        const address = accountAddress(accountId)
        return address.length > 0 ? title + " (" + address + ")" : title
    }

    // Gallery navigation owns row visuals; NkMail adds only mailbox counts.
    component MailboxAction: LiquidNavigationButton {
        id: action
        property string scopeHint: ""
        KosToolTip { target: action; text: action.scopeHint; onlyWhenTruncated: true; truncated: caption.truncated }
        property string countText: ""
        property string unreadText: ""
        Layout.fillWidth: true
        Layout.preferredHeight: 28
        specificationForm: true
        specificationFontSize: 12
        specificationSelectedVerticalInset: 2
        specificationSelectedFillColor: AppTheme.dark ? Qt.rgba(0.31, 0.56, 0.96, 0.28) : "#DCE9FB"
        specificationSelectedTextColor: AppTheme.dark ? "#EAF1FF" : AppTheme.accent
        specificationHoverFillColor: AppTheme.dark ? Qt.rgba(1, 1, 1, .08) : "#dbdbdd"
        font.weight: Font.Medium
        checkable: false
        checked: false
        iconColor: checked ? specificationSelectedTextColor : AppTheme.mutedText
        contentItem: Item {
            implicitWidth: caption.implicitWidth
                + (action.iconName.length ? 24 : 0)
                + (unread.visible ? unread.implicitWidth + 6 : 0)
            implicitHeight: Math.max(caption.implicitHeight, 16)
            VectorIcon {
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                visible: action.iconName.length > 0
                name: action.iconName; size: 14; color: action.iconColor
            }
            Text {
                id: caption
                anchors.left: parent.left; anchors.right: unread.visible ? unread.left : parent.right
                anchors.leftMargin: action.iconName.length ? 24 : 0
                anchors.rightMargin: unread.visible ? 6 : 0
                anchors.verticalCenter: parent.verticalCenter
                text: action.text
                textFormat: Text.PlainText
                color: action.checked ? action.specificationSelectedTextColor
                    : AppTheme.dark ? Qt.rgba(1, 1, 1, .92) : Qt.rgba(0, 0, 0, .92)
                font: action.font
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            Text {
                id: unread
                objectName: "mailboxUnread"
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                visible: text.length > 0
                text: action.unreadText
                font: action.font
                color: action.checked ? action.specificationSelectedTextColor : AppTheme.accent
                textFormat: Text.PlainText
            }
            Text {
                id: count
                objectName: "mailboxCount"
                visible: false
                text: action.countText
                font: action.font
                textFormat: Text.PlainText
            }
        }
    }

    // Sidebar account row: a disclosure chevron plus the account title. The
    // title colour distinguishes the active account from the others.
    component AccountHeader: LiquidNavigationButton {
        id: header
        property string accountId: ""
        property bool current: false
        property bool collapsed: false
        Layout.fillWidth: true
        Layout.preferredHeight: 28
        specificationForm: true
        specificationFontSize: 12
        specificationSelectedVerticalInset: 2
        specificationHoverFillColor: AppTheme.dark ? Qt.rgba(1, 1, 1, .08) : "#dbdbdd"
        checkable: false
        checked: false
        contentItem: Item {
            implicitHeight: Math.max(title.implicitHeight, 16)
            VectorIcon {
                id: chevron
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                name: "chevron"; size: 12
                color: header.current ? AppTheme.text : AppTheme.mutedText
                rotation: header.collapsed ? -90 : 0
                Behavior on rotation {
                    NumberAnimation { duration: AppTheme.reduceMotion ? 0 : AppTheme.motionFast }
                }
            }
            Text {
                id: title
                anchors.fill: parent; anchors.leftMargin: 24
                text: header.text
                color: header.current ? AppTheme.text : AppTheme.mutedText
                font.pixelSize: 13; font.weight: Font.DemiBold
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            Text {
                id: accountUnread
                objectName: "accountUnread_" + header.accountId
                visible: false
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                text: {
                    root.unreadRevision
                    root.mail.accountUnreadRevision
                    const count = root.mail.accountUnread(header.accountId)
                    return count > 0 ? String(count) : ""
                }
                color: AppTheme.mutedText; font.pixelSize: 11
                textFormat: Text.PlainText
            }
        }
    }


        objectName: "sidebarBody"
        Flickable {
            id: mailboxNavigation
            objectName: "mailboxNavigation"
            anchors.fill: parent
            anchors.leftMargin: 14; anchors.rightMargin: 14
            anchors.bottomMargin: 14; anchors.topMargin: 4
            clip: true
            contentWidth: width
            contentHeight: mailboxColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            ColumnLayout {
                id: mailboxColumn
                width: parent.width
                spacing: 0
                Text {
                    objectName: "favoritesTitle"
                    text: qsTr("Favorites"); color: root.muted
                    font.pixelSize: 11; font.weight: Font.DemiBold
                    topPadding: 8; bottomPadding: 6
                }
                Repeater {
                    model: ["inbox", "flagged", "localDrafts", "sent"]
                    delegate: MailboxAction {
                        required property string modelData
                        readonly property var targetFolder: Navigation.systemFolder(mail.folders || [], modelData)
                        objectName: modelData === "localDrafts" ? "draftFolderButton" : "favoriteShortcut_" + modelData
                        readonly property string mailboxTitle: modelData === "flagged" ? MailLabels.smartMailboxTitle("flagged")
                            : modelData === "localDrafts" ? qsTr("Local drafts")
                            : modelData === "inbox" ? qsTr("Inbox") : qsTr("Sent")
                        text: root.scopedTitle(mailboxTitle, mail.currentAccountId)
                        scopeHint: text
                        iconName: modelData === "flagged" ? "flag" : modelData === "localDrafts" ? "editRounded"
                            : modelData === "inbox" ? "envelope" : "forward"
                        enabled: modelData === "flagged" || modelData === "localDrafts" || !!targetFolder
                        unreadText: {
                            root.unreadRevision
                            if (modelData === "localDrafts") return mail.drafts.length > 0 ? String(mail.drafts.length) : ""
                            const count = targetFolder ? (mail.folderUnread()[targetFolder.id] || 0) : 0
                            return count > 0 ? String(count) : ""
                        }
                        checked: modelData === "localDrafts" ? root.draftsVisible
                            : !root.draftsVisible && (modelData === "flagged" ? mail.smartMailbox === "flagged"
                            : root.navigationOrigin === "favorite" && !mail.smartMailbox && !!targetFolder && mail.currentFolder === targetFolder.id)
                        onClicked: {
                            root.navigationOrigin = "favorite"
                            if (modelData === "localDrafts") { root.draftsRequested(); return }
                            root.mailboxOpened()
                            if (modelData === "flagged") mail.selectSmartMailbox("flagged")
                            else if (targetFolder) mail.selectFolder(targetFolder.id)
                        }
                    }
                }
                Repeater {
                    objectName: "favoritesList"
                    model: mail.favorites || []
                    delegate: MailboxAction {
                        id: favoriteRow
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: favoriteMenu.openAtAnchor()
                        }
                        LiquidPopover {
                            id: favoriteMenu
                            sourceItem: root.surface
                            anchorItem: favoriteRow
                            LiquidPopoverItem {
                                text: qsTr("从收藏移除"); iconName: "heart"
                                onTriggered: {
                                    favoriteMenu.close()
                                    mail.toggleFavorite(favoriteRow.modelData.accountId, favoriteRow.modelData.folderId, favoriteRow.modelData.title)
                                }
                            }
                        }
                        required property var modelData
                        text: root.scopedTitle(modelData.title, modelData.accountId)
                        scopeHint: text
                        iconName: MailLabels.folderIcon(modelData.folderId)
                        unreadText: {
                            root.unreadRevision
                            const counts = mail.folderUnreadFor(modelData.accountId)
                            return counts[modelData.folderId] > 0 ? String(counts[modelData.folderId]) : ""
                        }
                        checked: root.navigationOrigin === "custom" && !mail.smartMailbox && mail.currentAccountId === modelData.accountId && !root.draftsVisible && mail.currentFolder === modelData.folderId
                        onClicked: { root.navigationOrigin = "custom"; root.mailboxOpened(); mail.openFavorite(modelData.accountId, modelData.folderId) }
                    }
                }
                // Deferred until grouping, scope and configurable rules are designed.
                ColumnLayout {
                    objectName: "deferredSmartMailboxes"
                    visible: false
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        objectName: "smartMailboxesTitle"
                        text: qsTr("Smart mailboxes"); color: root.muted
                        font.pixelSize: 11; font.weight: Font.DemiBold
                        topPadding: 16; bottomPadding: 4
                    }
                    Text {
                        text: qsTr("Cached mail")
                        color: root.muted; font.pixelSize: 9
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                        bottomPadding: 6
                    }
                    Repeater {
                        model: ["allInbox", "unread", "attachments"]
                        delegate: MailboxAction {
                            required property string modelData
                            objectName: "smartMailbox_" + modelData
                            text: MailLabels.smartMailboxTitle(modelData)
                            iconName: modelData === "attachments" ? "copy" : "envelope"
                            checked: !root.draftsVisible && mail.smartMailbox === modelData
                            onClicked: { root.mailboxOpened(); mail.selectSmartMailbox(modelData) }
                        }
                    }
                }
                Text {
                    objectName: "accountsTitle"
                    text: qsTr("Accounts"); color: root.muted
                    font.pixelSize: 11; font.weight: Font.DemiBold
                    topPadding: 20; bottomPadding: 6
                }
                Repeater {
                    model: root.sidebarAccounts
                    delegate: ColumnLayout {
                        id: accountSection
                        required property var modelData
                        readonly property string accountId: modelData.id
                        readonly property bool current: root.mail.currentAccountId === accountId
                        readonly property bool collapsed: root.collapsedAccounts[accountId] === true
                        Layout.fillWidth: true
                        spacing: 0
                        AccountHeader {
                            id: accountRow
                            objectName: "accountHeader_" + accountSection.accountId
                            accountId: accountSection.accountId
                            current: accountSection.current
                            collapsed: accountSection.collapsed
                            text: accountSection.modelData.address || accountSection.modelData.name
                            Accessible.name: text
                            onClicked: {
                                root.toggleAccountCollapsed(accountSection.accountId)
                            }
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: accountMenu.openAtAnchor()
                            }
                            LiquidPopover {
                                id: accountMenu
                                objectName: "accountMenu"
                                sourceItem: root.surface
                                anchorItem: accountRow
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                                LiquidPopoverItem {
                                    text: qsTr("编辑账号"); iconName: "settings"
                                    onTriggered: {
                                        accountMenu.close()
                                        root.editAccountRequested(accountSection.modelData)
                                    }
                                }
                                LiquidPopoverItem {
                                    text: qsTr("删除账号"); iconName: "trash"
                                    onTriggered: {
                                        accountMenu.close()
                                        root.removeAccountRequested(accountSection.accountId, accountSection.modelData.name || accountSection.modelData.address || "")
                                    }
                                }
                            }
                        }
                        RowLayout {
                            visible: accountSection.current && (mail.busy || !!mail.error)
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            spacing: 6
                            MailActivityIndicator {
                                objectName: "accountSyncSpinner_" + accountSection.accountId
                                visible: mail.busy
                                running: mail.busy
                                Layout.preferredWidth: 14; Layout.preferredHeight: 14
                            }
                            Text {
                                objectName: "accountSyncStatus_" + accountSection.accountId
                                text: mail.error ? mail.error : mail.status
                                Layout.fillWidth: true
                                font.pixelSize: 10
                                color: mail.error ? AppTheme.destructive : AppTheme.mutedText
                                wrapMode: Text.Wrap
                            }
                        }
                        Repeater {
                            // Synchronous cache read; it is not re-evaluated when
                            // the active account changes, only when the section folds.
                            objectName: accountSection.current ? "mailboxFolders" : "mailboxFolders_" + accountSection.accountId
                            model: accountSection.collapsed ? [] : (root.unreadRevision, Navigation.ordered(accountSection.current ? root.mail.folders : (root.mail.foldersFor(accountSection.accountId) || [])))
                            delegate: MailboxAction {
                                id: folderRow
                                Layout.leftMargin: 18
                                required property var modelData
                                // Keep the historical name for the active account; scope
                                // other accounts so their identical folder ids stay unique.
                                objectName: accountSection.current
                                    ? "mailboxFolder_" + modelData.id
                                    : "mailboxFolder_" + accountSection.accountId + "_" + modelData.id
                                text: MailLabels.mailboxTitle(modelData)
                                iconName: MailLabels.mailboxIcon(modelData)
                                countText: ""
                                unreadText: {
                                    root.unreadRevision
                                    const counts = accountSection.current ? root.mail.folderUnread() : root.mail.folderUnreadFor(accountSection.accountId)
                                    const count = counts[modelData.id] || 0
                                    return count > 0 ? String(count) : ""
                                }
                                checked: root.navigationOrigin === "account" && !mail.smartMailbox && accountSection.current && !root.draftsVisible && root.mail.currentFolder === modelData.id
                                Accessible.name: MailLabels.mailboxTitle(modelData)
                                onClicked: {
                                    if (!accountSection.current) {
                                        root.mail.switchAccount(accountSection.accountId)
                                        if (root.mail.currentAccountId !== accountSection.accountId) return
                                    }
                                    root.navigationOrigin = "account"
                                    root.mailboxOpened()
                                    root.mail.selectFolder(modelData.id)

                                }
                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: folderMenu.openAtAnchor()
                                }
                                LiquidPopover {
                                    id: folderMenu
                                    objectName: "folderMenu"
                                    sourceItem: root.surface
                                    anchorItem: folderRow
                                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
                                    LiquidPopoverItem {
                                        // Touching mail.favorites keeps the label in sync when
                                        // the list changes; isFavorite() itself is not tracked.
                                        text: (mail.favorites, mail.isFavorite(accountSection.accountId, folderRow.modelData.id))
                                            ? qsTr("从收藏移除") : qsTr("添加到收藏")
                                        iconName: "heart"
                                        onTriggered: {
                                            mail.toggleFavorite(accountSection.accountId, folderRow.modelData.id,
                                                MailLabels.mailboxTitle(folderRow.modelData))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                MailAction {
                    id: addAccountAction
                    objectName: "addAccountButton"
                    text: qsTr("添加账号")
                    iconName: "plus"
                    contentItem: Item {
                        implicitWidth: addAccountContent.implicitWidth
                        implicitHeight: addAccountContent.implicitHeight
                        Row {
                            id: addAccountContent
                            anchors.centerIn: parent
                            spacing: 6
                            VectorIcon { name: "plus"; size: 14; color: addAccountAction.textColor; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                objectName: "addAccountCaption"
                                text: addAccountAction.text; color: addAccountAction.textColor
                                font: addAccountAction.font
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                    primaryAction: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    Layout.topMargin: 12
                    onClicked: root.addAccountRequested()
                }
            }
        }
}

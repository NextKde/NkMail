import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui
import "ReaderLayout.js" as ReaderLayout

Rectangle {
    id: root
    property string localeName: Qt.locale().name
    required property var mail
    property var message: ({})
    property bool hasMessage: false
    readonly property bool bodyPending: message.bodyPending === true
    readonly property bool bodyLoading: message.bodyLoading === true
    readonly property string bodyError: message.bodyError || ""
    readonly property bool hasHeader: !!(message.sender || message.senderAddress || message.subject || message.date || message.to)
    readonly property bool renderingBody: browserBody.active && (!browserBody.item || !browserBody.item.documentReady)
    property bool draftsVisible: false
    property bool detailsVisible: false
    property bool showRichText: true
    property bool compact: false
    readonly property color ink: AppTheme.text
    readonly property color muted: AppTheme.mutedText
    readonly property color rule: AppTheme.border
    signal reconnectRequested()
    signal closeDetailsRequested()
    signal detailsRequested()
    signal saveAttachmentRequested(int index)

    objectName: "messageReader"
    color: "transparent"
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        ScrollView {
            id: readerScroll
            objectName: "readerScroll"
            visible: root.hasMessage && !root.draftsVisible
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth; clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: readerScroll.availableWidth
                spacing: 0
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: false
                    Layout.preferredWidth: Math.max(0, Math.min(MailStyle.readerMaximumWidth, readerScroll.availableWidth - (root.compact ? 48 : 72)))
                    Layout.maximumWidth: Math.max(0, Math.min(MailStyle.readerMaximumWidth, readerScroll.availableWidth - (root.compact ? 48 : 72)))
                    Layout.minimumWidth: 0
                    spacing: 14
                    MailMessageHeader {
                        objectName: "mailMessageHeader"
                        visible: root.hasHeader
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.topMargin: 12
                        message: root.message
                        localeName: root.localeName
                        sourceLabel: (root.mail.account.name || root.mail.account.address || "") + " · " + MailLabels.mailboxTitle({id: root.message.folder || root.mail.currentFolder})
                        expanded: root.detailsVisible
                        onToggleDetails: root.detailsVisible ? root.closeDetailsRequested() : root.detailsRequested()
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: root.rule; Layout.topMargin: 4; Layout.bottomMargin: 6 }
                    Item {
                        id: detailsPanel
                        objectName: "messageDetailsPanel"
                        Layout.fillWidth: true
                        readonly property real contentHeight: Math.max(140, Math.min(380, detailsColumn.implicitHeight + 40))
                        visible: root.detailsVisible
                        implicitHeight: visible ? contentHeight : 0
                        Layout.preferredHeight: implicitHeight
                        clip: true
                        RowLayout {
                            id: detailsHeader
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 12
                            height: 28
                            spacing: 8
                            Text { text: qsTr("Message details"); color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold; Layout.fillWidth: true }
                            MailAction { iconOnly: true; text: "×"; font.pixelSize: 20; accessibleText: qsTr("Close message details"); onClicked: root.closeDetailsRequested() }
                        }
                        ScrollView {
                            id: detailsScroll
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: detailsHeader.bottom
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 8
                            anchors.bottomMargin: 12
                            contentWidth: availableWidth
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            ColumnLayout {
                                id: detailsColumn
                                width: parent.width
                                spacing: 8
                                Repeater {
                                    model: [ {title: qsTr("From"), value: MailLabels.value(root.message, "senderAddress")},
                                        {title: qsTr("To"), value: MailLabels.value(root.message, "to")},
                                        {title: qsTr("Cc"), value: MailLabels.value(root.message, "cc")},
                                        {title: qsTr("Date"), value: MailLabels.dateLabel(root.message.date, root.localeName)},
                                        {title: qsTr("Message ID"), value: MailLabels.value(root.message, "messageId")} ]
                                    delegate: ColumnLayout {
                                        id: detailsEntry
                                        required property var modelData
                                        visible: modelData.value.length > 0
                                        Layout.fillWidth: true; spacing: 3
                                        Text { text: detailsEntry.modelData.title; color: root.muted; font.pixelSize: 11 }
                                        TextEdit { text: detailsEntry.modelData.value; color: root.ink; font.pixelSize: 13; readOnly: true; selectByMouse: true; textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true }
                                    }
                                }
                                Text { text: qsTr("Reading does not change server flags."); color: root.muted; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
                            }
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: root.rule
                        }
                    }

                    RowLayout {
                        objectName: "restoreFormattingBar"
                        Layout.fillWidth: true
                        spacing: 12
                        visible: root.showRichText && !root.mail.demo
                            && Number(MailLabels.value(root.message, "bodyFormatVersion") || 0) < 1
                            && MailLabels.value(root.message, "html").length > 0
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Connect to load the original message formatting.")
                            color: root.muted; font.pixelSize: 12; wrapMode: Text.Wrap
                        }
                        KosButton {
                            objectName: "restoreFormattingButton"
                            text: qsTr("Connect account")
                            secondary: true; implicitHeight: 30
                            onClicked: root.reconnectRequested()
                        }
                    }
                    RowLayout {
                        objectName: "blockedImagesBar"
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.mail.blockedImageCount > 0
                        Text {
                            text: qsTr("%1 张远程图片已隐藏").arg(root.mail.blockedImageCount)
                            color: root.muted; font.pixelSize: 11
                            Layout.fillWidth: true
                        }
                        MailAction {
                            stacked: true; stackedText: qsTr("显示图片"); iconName: "download"
                            accessibleText: qsTr("Load remote images")
                            onClicked: root.mail.loadRemoteImages()
                        }
                    }
                    ColumnLayout {
                        objectName: "messageBodyLoadingState"
                        visible: root.bodyLoading || root.renderingBody || root.bodyError.length > 0
                        Layout.fillWidth: true
                        Layout.topMargin: 20; Layout.bottomMargin: 20
                        spacing: 12
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 12
                            MailActivityIndicator {
                                objectName: "messageBodySpinner"
                                running: root.bodyLoading || root.renderingBody
                                visible: running
                                implicitWidth: 32; implicitHeight: 32
                            }
                            Text {
                                text: root.bodyError.length ? qsTr("Could not load message")
                                    : root.bodyLoading ? qsTr("Downloading message…") : qsTr("Rendering message…")
                                color: root.ink; font.pixelSize: 14; font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            visible: root.bodyError.length > 0
                            text: root.bodyError
                            Layout.fillWidth: true
                            color: root.muted; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                        }
                        KosButton {
                            objectName: "retryMessageBodyButton"
                            visible: root.bodyError.length > 0 && !root.bodyLoading
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Retry")
                            onClicked: root.mail.openMessage(String(root.message.uid || ""), String(root.message.uidValidity || ""), String(root.message.folder || ""))
                        }
                    }
                    // Render only the backend-sanitized html field; fall back to plain text.
                    Loader {
                        id: browserBody
                        objectName: "browserBodyLoader"
                        readonly property string displayHtml: MailLabels.value(root.message, "displayHtml")
                        active: !root.bodyPending && root.showRichText && displayHtml.length > 0
                        visible: active
                        Layout.fillWidth: true
                        Layout.preferredHeight: item ? item.implicitHeight : 0
                        sourceComponent: Component {
                            MailHtmlBody {
                                html: browserBody.displayHtml
                                onLinkActivated: function(link) {
                                    linkClip.text = link
                                    linkClip.selectAll(); linkClip.copy()
                                    root.mail.clearError()
                                    statusHint.show(qsTr("Link copied to clipboard"))
                                }
                            }
                        }
                    }
                    // Keep formats fixed: switching a single TextEdit between
                    // plain and rich modes can briefly parse plain mail as HTML.
                    TextEdit {
                        id: messageBody
                        objectName: "messageBody"
                        Layout.preferredWidth: 0
                        Layout.minimumWidth: 0
                        visible: !root.bodyPending && !browserBody.active && !asRich
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(200, contentHeight)
                        readonly property string richHtml: MailLabels.value(root.message, "html") || ""
                        readonly property bool asRich: root.showRichText && String(richHtml).length > 0
                        text: MailLabels.value(root.message, "text")
                        textFormat: TextEdit.PlainText
                        readOnly: true; selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        color: root.ink
                        selectionColor: AppTheme.accent; selectedTextColor: "white"
                        font.pixelSize: 14
                        onLinkActivated: function(link) {
                            if (!/^(https?:|mailto:)/i.test(String(link))) return
                            linkClip.text = String(link)
                            linkClip.selectAll(); linkClip.copy()
                            mail.clearError()
                            statusHint.show(qsTr("Link copied to clipboard"))
                        }
                    }
                    TextEdit {
                        id: legacyMessageBody
                        objectName: "legacyMessageBody"
                        Layout.preferredWidth: 0
                        Layout.minimumWidth: 0
                        visible: !root.bodyPending && !browserBody.active && asRich
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(200, contentHeight)
                        readonly property string richHtml: MailLabels.value(root.message, "html") || ""
                        readonly property bool asRich: root.showRichText && String(richHtml).length > 0
                        // Defer width-dependent image fitting until the layout settles.
                        // A synchronous text/implicit-width/layout binding forms a loop.
                        function renderLegacy() { text = asRich ? ReaderLayout.readableHtml(richHtml, width) : "" }
                        onRichHtmlChanged: legacyRenderTimer.restart()
                        onAsRichChanged: legacyRenderTimer.restart()
                        onWidthChanged: legacyRenderTimer.restart()
                        Component.onCompleted: legacyRenderTimer.restart()
                        Timer { id: legacyRenderTimer; interval: 0; onTriggered: legacyMessageBody.renderLegacy() }
                        textFormat: TextEdit.RichText
                        readOnly: true; selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        color: root.ink
                        selectionColor: AppTheme.accent; selectedTextColor: "white"
                        font.pixelSize: 14
                        onLinkActivated: function(link) {
                            if (!/^(https?:|mailto:)/i.test(String(link))) return
                            linkClip.text = String(link)
                            linkClip.selectAll(); linkClip.copy()
                            mail.clearError()
                            statusHint.show(qsTr("Link copied to clipboard"))
                        }
                    }
                    TextEdit { id: linkClip; visible: false }
                    Text {
                        id: statusHint
                        visible: false
                        text: ""
                        color: root.muted; font.pixelSize: 11
                        function show(message) { text = message; visible = true; hintTimer.restart() }
                        Timer { id: hintTimer; interval: 2500; onTriggered: statusHint.visible = false }
                    }
                    Repeater {
                        model: root.message.attachments || []
                        delegate: Rectangle {
                            id: attachmentRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true; implicitHeight: 48
                            radius: 6; color: AppTheme.contentSecondaryBackground
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10
                                VectorIcon { name: "album"; size: 18; color: root.muted }
                                Text { text: attachmentRow.modelData.name; color: root.ink; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                                MailAction { stacked: true; stackedText: qsTr("保存"); iconName: "download"; accessibleText: qsTr("Save attachment"); onClicked: { root.saveAttachmentRequested(attachmentRow.index) } }
                            }
                        }
                    }
                }
            }
        }
        Item {
            visible: !root.hasMessage || root.draftsVisible
            Layout.fillWidth: true; Layout.fillHeight: true
            Column {
                anchors.centerIn: parent; spacing: 12; width: Math.min(310, parent.width - 48)
                Text { width: parent.width; text: root.draftsVisible ? qsTr("Pick up where you left off.") : qsTr("A moment of quiet."); color: root.muted; font.pixelSize: 22; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                Text { width: parent.width; text: root.draftsVisible ? qsTr("Choose a draft to continue writing.") : qsTr("Select a message to read it here."); color: root.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
            }
        }
    }
}

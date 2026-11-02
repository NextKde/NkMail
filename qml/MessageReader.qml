import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

Rectangle {
    id: root
    property string localeName: Qt.locale().name
    required property var mail
    property var message: ({})
    property bool hasMessage: false
    property bool draftsVisible: false
    property bool detailsVisible: false
    property bool showRichText: true
    property bool compact: false
    readonly property color ink: AppTheme.text
    readonly property color muted: AppTheme.mutedText
    readonly property color rule: AppTheme.border
    signal closeDetailsRequested()
    signal saveAttachmentRequested(int index)
    signal addAccountRequested()

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
                            MailAction { stacked: true; stackedText: qsTr("关闭"); iconName: "close"; accessibleText: qsTr("Close message details"); onClicked: root.closeDetailsRequested() }
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
                    Text {
                        text: MailLabels.value(root.message, "subject") || qsTr("(No subject)")
                        color: root.ink; font.pixelSize: 25; font.weight: Font.Bold
                        wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 12
                        Rectangle {
                            width: 36; height: 36; radius: 18; color: MailStyle.avatar
                            Text { anchors.centerIn: parent; text: (MailLabels.value(root.message, "sender") || "?").slice(0, 1).toUpperCase(); color: AppTheme.accent; font.pixelSize: 16; font.weight: Font.DemiBold }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            Text { text: MailLabels.value(root.message, "sender") || MailLabels.value(root.message, "senderAddress"); color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                            Text { text: qsTr("To: ") + MailLabels.value(root.message, "to"); color: root.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                        }
                    }
                    Text { text: MailLabels.dateLabel(root.message.date, root.localeName); color: root.muted; font.pixelSize: 11; Layout.fillWidth: true }
                    Rectangle { Layout.fillWidth: true; height: 1; color: root.rule; Layout.topMargin: 4; Layout.bottomMargin: 6 }
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
                    // Render only the backend-sanitized html field; fall back to plain text.
                    TextEdit {
                        id: messageBody
                        objectName: "messageBody"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(200, contentHeight)
                        readonly property string richHtml: MailLabels.value(root.message, "html") || ""
                        readonly property bool asRich: root.showRichText && String(richHtml).length > 0
                        text: asRich ? String(richHtml) : MailLabels.value(root.message, "text")
                        textFormat: asRich ? TextEdit.RichText : TextEdit.PlainText
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
                MailAction { anchors.horizontalCenter: parent.horizontalCenter; visible: !mail.configured && !mail.demo; text: qsTr("Add an account"); onClicked: root.addAccountRequested() }
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts
import Kos.Ui

// Presentation only: message loading and details state belong to the reader.
GridLayout {
    id: root
    required property var message
    property string localeName: Qt.locale().name
    property string sourceLabel: ""
    property bool expanded: false
    readonly property bool compact: width < 360
    signal toggleDetails()
    columns: compact ? 1 : 2
    columnSpacing: 16
    rowSpacing: 6
    RowLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 10
        Rectangle {
            Layout.alignment: Qt.AlignTop
            width: 30; height: 30; radius: 15
            color: MailStyle.avatar
            Text {
                anchors.centerIn: parent
                text: (MailLabels.value(root.message, "sender") || MailLabels.value(root.message, "senderAddress") || "?").slice(0, 1).toUpperCase()
                color: AppTheme.accent; font.pixelSize: 14; font.weight: Font.DemiBold
                textFormat: Text.PlainText
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 3
            Text {
                objectName: "readerSender"
                text: MailLabels.value(root.message, "sender") || MailLabels.value(root.message, "senderAddress")
                Layout.fillWidth: true; Layout.minimumWidth: 0
                font.pixelSize: 13; font.weight: Font.DemiBold
                color: AppTheme.text; elide: Text.ElideRight; textFormat: Text.PlainText
            }
            Text {
                objectName: "readerSubject"
                text: MailLabels.value(root.message, "subject") || qsTr("(No subject)")
                Layout.fillWidth: true; Layout.minimumWidth: 0
                font.pixelSize: 14; font.weight: Font.Medium
                color: AppTheme.text; wrapMode: Text.Wrap; textFormat: Text.PlainText
            }
            Text {
                text: qsTr("To: ") + MailLabels.value(root.message, "to")
                Layout.fillWidth: true; Layout.minimumWidth: 0
                font.pixelSize: 11; color: AppTheme.mutedText
                elide: Text.ElideRight; textFormat: Text.PlainText
            }
        }
    }
    ColumnLayout {
        Layout.alignment: Qt.AlignTop | Qt.AlignRight
        Layout.fillWidth: root.compact
        Layout.maximumWidth: root.compact ? root.width : 145
        Layout.minimumWidth: 0
        spacing: 3
        Text {
            text: root.sourceLabel
            visible: text.length > 0
            Layout.fillWidth: true; Layout.minimumWidth: 0
            horizontalAlignment: Text.AlignRight
            color: AppTheme.mutedText; font.pixelSize: 10
            elide: Text.ElideRight; textFormat: Text.PlainText
        }
        Text {
            text: MailLabels.dateLabel(root.message.date, root.localeName)
            Layout.fillWidth: true; Layout.minimumWidth: 0
            horizontalAlignment: Text.AlignRight
            color: AppTheme.mutedText; font.pixelSize: 10
            elide: Text.ElideRight; textFormat: Text.PlainText
        }
        LiquidActionButton {
            objectName: "messageDetailsButton"
            Layout.alignment: Qt.AlignRight
            text: root.expanded ? qsTr("Hide details") : qsTr("Details")
            implicitHeight: 20
            materialForm: true; cornerRadius: 6; flat: true
            font.pixelSize: 11
            Accessible.name: text
            onClicked: root.toggleDetails()
        }
    }
}

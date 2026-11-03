import QtQuick
import QtQuick.Layouts
import Kos.Ui
import "ProviderProfiles.js" as Profiles

ColumnLayout {
    id: root
    property int currentIndex: 0
    readonly property string provider: Profiles.ids[currentIndex] || "manual"
    readonly property bool available: Profiles.settings(provider).available
    readonly property var labels: [qsTr("Other mail account"), "iCloud", "Google / Gmail", "QQ Mail", qsTr("Tencent Exmail"), "163", "126", "Yahoo", "AOL", "Microsoft / Exchange"]
    readonly property string guidance: provider === "microsoft"
        ? qsTr("Microsoft OAuth and native Exchange are not available yet. Browser sign-in requires an application registration.")
        : provider === "google" ? qsTr("Use a Google app password with two-step verification. Some managed accounts require OAuth, which is not available yet.")
        : ['qq','exmail','163','126'].indexOf(provider) >= 0
            ? qsTr("Enable IMAP/SMTP in your webmail settings and use the client authorization code when required.")
        : provider === "manual" ? qsTr("Enter the IMAP and SMTP settings supplied by your mail administrator.")
        : qsTr("Use an app-specific password generated in your provider account settings.")
    spacing: 8
    Text { text: qsTr("Choose your provider"); color: AppTheme.text; font.pixelSize: 13; font.weight: Font.DemiBold }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: rows.implicitHeight + 12
        radius: 12
        color: AppTheme.contentSecondaryBackground
        ColumnLayout {
            id: rows
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: 6
            spacing: 0
            Repeater {
                model: [1,2,3,4,5,6,7,8,9,0]
                delegate: LiquidNavigationButton {
                    required property int modelData
                    objectName: "provider_" + Profiles.ids[modelData]
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    specificationForm: true
                    specificationFontSize: 13
                    text: root.labels[modelData] + (modelData === 9 ? qsTr(" · Not available yet") : "")
                    iconName: "envelope"
                    checked: root.currentIndex === modelData
                    onClicked: root.currentIndex = modelData
                }
            }
        }
    }
    Text { text: root.guidance; Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: AppTheme.mutedText }
}

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
                    id: providerRow
                    required property int modelData
                    objectName: "provider_" + Profiles.ids[modelData]
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    specificationForm: true
                    specificationFontSize: 13
                    text: root.labels[modelData] + (modelData === 9 ? qsTr(" · Not available yet") : "")
                    // Preserve brand colors and wordmark aspect ratios; the shared
                    // button still owns selection, focus, hover and input behavior.
                    contentItem: RowLayout {
                        spacing: 10
                        Item {
                            Layout.preferredWidth: 52
                            Layout.fillHeight: true
                            Rectangle {
                                anchors.centerIn: parent
                                width: 56; height: 20; radius: 3
                                color: "white"
                                visible: AppTheme.dark && (providerRow.modelData === 5 || providerRow.modelData === 6)
                            }
                            Image {
                                objectName: "providerLogo_" + Profiles.ids[providerRow.modelData]
                                anchors.centerIn: parent
                                width: providerRow.modelData === 5 || providerRow.modelData === 6 ? 52 : 24
                                height: 24
                                source: Profiles.logo(Profiles.ids[providerRow.modelData])
                                fillMode: Image.PreserveAspectFit
                                visible: source.toString().length > 0
                                smooth: true
                            }
                            VectorIcon {
                                anchors.centerIn: parent
                                visible: providerRow.modelData === 0
                                name: "envelope"; size: 20; color: AppTheme.accent
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: providerRow.text
                            font: providerRow.font
                            color: AppTheme.text
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    checked: root.currentIndex === modelData
                    onClicked: root.currentIndex = modelData
                }
            }
        }
    }
    Text { text: root.guidance; Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: AppTheme.mutedText }
}

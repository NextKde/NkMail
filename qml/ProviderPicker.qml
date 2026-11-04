import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui
import "ProviderProfiles.js" as Profiles

ColumnLayout {
    id: root
    property int currentIndex: 0
    readonly property string provider: Profiles.ids[currentIndex] || "manual"
    readonly property bool available: Profiles.settings(provider).available
    readonly property var labels: [qsTr("Other mail account"), "iCloud", "Google / Gmail", "QQ Mail", qsTr("Tencent Exmail"), "163", "126", "Yahoo", "AOL"]
    readonly property string guidance: provider === "google" ? qsTr("Enable two-step verification in your Google account and generate an app password. Use that app password here, not your Google account password.")
        : ['qq','exmail','163','126'].indexOf(provider) >= 0
            ? qsTr("Enable IMAP or POP and SMTP in your webmail settings and use the client authorization code when required.")
        : provider === "manual" ? qsTr("Enter the IMAP or POP and SMTP settings supplied by your mail administrator.")
        : qsTr("Use an app-specific password generated in your provider account settings.")
    spacing: 8
    Text { text: qsTr("Choose your provider"); color: AppTheme.text; font.pixelSize: 13; font.weight: Font.DemiBold }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Math.min(rows.implicitHeight, 192) + 12
        radius: 12
        color: AppTheme.contentSecondaryBackground
        ScrollView {
            id: providerScroll
            objectName: "providerScroll"
            anchors.fill: parent
            anchors.margins: 6
            contentWidth: availableWidth
            rightPadding: 12
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                objectName: "providerScrollBar"
                parent: providerScroll
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                orientation: Qt.Vertical
                policy: ScrollBar.AlwaysOn
                active: true
                interactive: true
                Accessible.name: qsTr("Choose your provider")
            }
            ColumnLayout {
                id: rows
                width: providerScroll.availableWidth
                spacing: 0
                Repeater {
                    model: Profiles.ids.map((_, index) => index).slice(1).concat([0])
                    delegate: LiquidNavigationButton {
                        id: providerRow
                        required property int modelData
                        objectName: "provider_" + Profiles.ids[modelData]
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        specificationForm: true
                        specificationFontSize: 13
                        text: root.labels[modelData]
                        // Preserve brand colors and wordmark aspect ratios; the shared
                        // button still owns selection, focus, hover and input behavior.
                        contentItem: RowLayout {
                            spacing: 10
                            Item {
                                Layout.preferredWidth: 28
                                Layout.fillHeight: true
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 24; height: 24; radius: 4
                                    color: "white"
                                    visible: providerRow.modelData === 5 || providerRow.modelData === 6
                                }
                                Image {
                                    objectName: "providerLogo_" + Profiles.ids[providerRow.modelData]
                                    anchors.centerIn: parent
                                    width: providerRow.modelData === 5 || providerRow.modelData === 6 ? 22 : 24
                                    height: 24
                                    // Show only the compact numeric brand mark, not the
                                    // adjacent website/tagline from the original asset.
                                    sourceClipRect: providerRow.modelData === 5 ? Qt.rect(0, 0, 122, 60)
                                        : providerRow.modelData === 6 ? Qt.rect(0, 0, 123, 62) : Qt.rect(0, 0, 0, 0)
                                    source: Profiles.logo(Profiles.ids[providerRow.modelData])
                                    fillMode: Image.PreserveAspectFit
                                    visible: source.toString().length > 0
                                    smooth: true
                                }
                                VectorIcon {
                                    anchors.centerIn: parent
                                    visible: providerRow.modelData === 0
                                    name: "envelope"; size: 28; color: AppTheme.accent
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
                        autoExclusive: true
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: root.currentIndex === modelData
                        checked: root.currentIndex === modelData
                        onClicked: root.currentIndex = modelData
                    }
                }
            }
        }
    }
    Text { text: root.guidance; Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: AppTheme.mutedText }
}

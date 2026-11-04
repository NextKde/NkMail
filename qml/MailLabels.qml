pragma Singleton
import QtQuick
import Kos.Ui
import "MailboxNavigation.js" as Navigation
QtObject {
    function smartMailboxTitle(kind) {
        if (kind === "allInbox") return qsTr("All inboxes (cached)")
        if (kind === "flagged") return qsTr("Flagged")
        if (kind === "unread") return qsTr("Unread")
        if (kind === "attachments") return qsTr("With attachments")
        return ""
    }
    function value(map, key) { return map && map[key] !== undefined ? String(map[key]) : "" }
    function mailboxTitle(folder) {
        const role = Navigation.role(folder)
        const labels = {all: qsTr("All mail"), inbox: qsTr("Inbox"), sent: qsTr("Sent"), drafts: qsTr("Server drafts"), archive: qsTr("Archive"), junk: qsTr("Junk"), trash: qsTr("Trash")}
        if (labels[role]) return labels[role]
        if (String(folder.id).toUpperCase() === "INBOX") return qsTr("Inbox")
        const title = String(folder.title || folder.id || "")
        if (title === "Sent") return qsTr("Sent")
        if (title === "Archive") return qsTr("Archive")
        if (title === "Junk") return qsTr("Junk")
        if (title === "Trash") return qsTr("Trash")
        return title
    }
    function mailboxIcon(folder) {
        const icons = {all:"archive",inbox:"envelope",sent:"forward",drafts:"editRounded",archive:"archive",junk:"flag",trash:"trash"}
        return icons[Navigation.role(folder)] || "folder"
    }
    function folderIcon(id) {
        const name = String(id || "").split(/[\/\\]/).pop().toLowerCase()
        const starts = (prefix) => name.indexOf(prefix) === 0
        if (starts("inbox") || name.indexOf("收件") >= 0) return "envelope"
        if (starts("sent") || name.indexOf("已发送") >= 0 || name.indexOf("发送") >= 0) return "forward"
        if (starts("draft") || name.indexOf("草稿") >= 0) return "editRounded"
        if (starts("archive") || name.indexOf("归档") >= 0) return "archive"
        if (starts("trash") || name.indexOf("删除") >= 0) return "trash"
        if (starts("junk") || name.indexOf("垃圾") >= 0) return "flag"
        return "folder"
    }
    function draftStateLabel(state) {
        switch (state) {
        case "draft": return qsTr("Draft")
        case "sending": return qsTr("Sending")
        case "accepted": return qsTr("Accepted")
        case "rejected": return qsTr("Rejected")
        case "unknown": return qsTr("Unknown")
        default: return String(state || "")
        }
    }
    function dateLabel(date, localeName) {
        if (date === "Yesterday") return qsTr("Yesterday")
        const d = new Date(date)
        if (isNaN(d.getTime())) return String(date || "")
        return Qt.formatDateTime(d, Qt.locale(localeName || Qt.locale().name), Locale.ShortFormat)
    }
}

pragma Singleton
import QtQuick
import Kos.Ui
QtObject {
    function value(map, key) { return map && map[key] !== undefined ? String(map[key]) : "" }
    function mailboxTitle(folder) {
        if (String(folder.id).toUpperCase() === "INBOX") return qsTr("Inbox")
        const title = String(folder.title || folder.id || "")
        if (title === "Sent") return qsTr("Sent")
        if (title === "Archive") return qsTr("Archive")
        if (title === "Junk") return qsTr("Junk")
        if (title === "Trash") return qsTr("Trash")
        return title
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

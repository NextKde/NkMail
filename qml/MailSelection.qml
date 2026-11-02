import QtQuick
QtObject {
    property var selectionIndices: ({})      // 行号 -> true
    property int selectionAnchor: -1
    readonly property int selectionSize: Object.keys(selectionIndices).length
    function isRowSelected(row) { return selectionIndices[row] === true }
    function clearSelection() { selectionIndices = ({}); selectionAnchor = -1 }
    function toggleRow(row) {
        const next = Object.assign({}, selectionIndices)
        if (next[row]) delete next[row]; else next[row] = true
        selectionIndices = next
        selectionAnchor = row
    }
    function selectRangeTo(row) {
        if (selectionAnchor < 0 || selectionAnchor === row) { toggleRow(row); return }
        const from = Math.min(selectionAnchor, row), to = Math.max(selectionAnchor, row)
        const next = Object.assign({}, selectionIndices)
        for (let i = from; i <= to; ++i) next[i] = true
        selectionIndices = next
    }
    function selectedIndexList() {
        const rows = Object.keys(selectionIndices).map(Number).sort((a, b) => a - b)
        return rows
    }
    property var collapsedThreads: ({})
    function toggleThread(key) {
        const next = Object.assign({}, collapsedThreads)
        if (next[key]) delete next[key]
        else next[key] = true
        collapsedThreads = next
    }
}

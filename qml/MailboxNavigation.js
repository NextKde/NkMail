.pragma library
function role(folder) {
    if (folder.role) return folder.role
    const name = String(folder.id || '').toLowerCase()
    const known = {inbox:'inbox',sent:'sent',drafts:'drafts',archive:'archive',archives:'archive',trash:'trash',junk:'junk'}
    return known[name] || ''
}
function ordered(folders) {
    const ranks = {inbox:0,drafts:1,sent:2,junk:3,trash:4,archive:5}
    return (folders || []).map((folder,index) => ({folder:folder,index:index})).sort((a,b) => {
        const ar = ranks[role(a.folder)], br = ranks[role(b.folder)]
        return (ar === undefined ? 6 : ar) - (br === undefined ? 6 : br) || a.index - b.index
    }).map(entry => entry.folder)
}

.pragma library
// Reviewed sources and scope: docs/PROVIDERS.md. Never guess custom-domain hosts.
var ids = ['manual', 'icloud', 'google', 'qq', 'exmail', '163', '126', 'yahoo', 'aol']
function forAddress(address) {
    const domain = String(address).trim().toLowerCase().split('@')[1]
    const domains = {'icloud.com':'icloud','me.com':'icloud','mac.com':'icloud',
        'gmail.com':'google','googlemail.com':'google','qq.com':'qq','foxmail.com':'qq',
        '163.com':'163','126.com':'126','yahoo.com':'yahoo','ymail.com':'yahoo','rocketmail.com':'yahoo',
        'aol.com':'aol','aim.com':'aol'}
    return domains[domain] || 'manual'
}
function indexForAddress(address) { return Math.max(0, ids.indexOf(forAddress(address))) }
function settings(provider) {
    const hosts = {icloud:['imap.mail.me.com','smtp.mail.me.com',587],
        google:['imap.gmail.com','smtp.gmail.com',465], qq:['imap.qq.com','smtp.qq.com',465],
        exmail:['imap.exmail.qq.com','smtp.exmail.qq.com',465],
        '163':['imap.163.com','smtp.163.com',465], '126':['imap.126.com','smtp.126.com',465],
        yahoo:['imap.mail.yahoo.com','smtp.mail.yahoo.com',465], aol:['imap.aol.com','smtp.aol.com',465]}
    const value = hosts[provider] || ['', '', 465]
    return {imapHost:value[0], imapPort:993, imapStartTls:false,
        smtpHost:value[1], smtpPort:value[2], smtpStartTls:value[2] === 587,
        serverSavesSent:provider === 'google', available:true}
}

// Packaged assets only: opening account setup must not contact providers.
function logo(provider) {
    const files = {icloud:'icloud.png', google:'google.svg', qq:'qq.png',
        exmail:'exmail.ico', '163':'163.png', '126':'126.png',
        yahoo:'yahoo.png', aol:'aol.png'}
    return files[provider] ? 'qrc:/data/providers/' + files[provider] : ''
}

function popHost(provider) {
    return ({google:'pop.gmail.com', qq:'pop.qq.com', exmail:'pop.exmail.qq.com',
        '163':'pop.163.com', '126':'pop.126.com', yahoo:'pop.mail.yahoo.com', aol:'pop.aol.com'})[provider] || ''
}

.pragma library
// Apple: https://support.apple.com/en-us/102525 (reviewed 2026-11-03).
// Unknown domains always remain manual; no guessed hosts or network discovery.
function forAddress(address) {
    const domain = address.trim().toLowerCase().split('@')[1]
    return ['icloud.com', 'me.com', 'mac.com'].indexOf(domain) >= 0 ? 'icloud' : 'manual'
}
function settings(provider) {
    return provider === 'icloud'
        ? { imapHost: 'imap.mail.me.com', imapPort: 993, smtpHost: 'smtp.mail.me.com', smtpPort: 587, smtpStartTls: true }
        : { imapHost: '', imapPort: 993, smtpHost: '', smtpPort: 465, smtpStartTls: false }
}

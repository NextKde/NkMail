import QtQuick
import QtWebEngine
import NextKde.Mail.Reader 1.0

WebEngineView {
    id: root
    required property string html
    readonly property bool retainedSurface: renderLifecycle.attached
    MailRenderLifecycle { id: renderLifecycle; view: root }
    property url documentUrl: ""
    property int documentHeight: 200
    property bool documentReady: false
    signal linkActivated(string link)
    objectName: "mailHtmlBody"
    implicitHeight: Math.max(200, Math.min(12000, documentHeight))
    backgroundColor: "white"
    audioMuted: true
    activeFocusOnPress: true
    profile: MailWebProfile {
        id: documentProfile
        onDownloadRequested: function(download) { download.cancel() }
    }
    settings.javascriptEnabled: false
    settings.javascriptCanOpenWindows: false
    settings.javascriptCanAccessClipboard: false
    settings.localStorageEnabled: false
    settings.localContentCanAccessFileUrls: false
    settings.localContentCanAccessRemoteUrls: false
    settings.pluginsEnabled: false
    settings.pdfViewerEnabled: false
    settings.fullScreenSupportEnabled: false
    settings.screenCaptureEnabled: false
    settings.navigateOnDropEnabled: false
    settings.hyperlinkAuditingEnabled: false
    settings.errorPageEnabled: false
    // Normal documents use the reader scrollbar; capped surfaces retain native
    // scrolling so the rest of an exceptionally long document remains reachable.
    settings.showScrollBars: documentHeight >= 12000
    settings.defaultTextEncoding: "UTF-8"
    onContextMenuRequested: function(request) { request.accepted = true }
    onNewWindowRequested: function(request) {
        if (/^(https?:|mailto:)/i.test(String(request.requestedUrl)))
            root.linkActivated(String(request.requestedUrl))
    }
    onPermissionRequested: function(permission) { permission.deny() }
    onNavigationRequested: function(request) {
        const link = String(request.url)
        if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation && /^(https?:|mailto:)/i.test(link))
            root.linkActivated(link)
        if (request.isMainFrame && (link === String(root.documentUrl) || link === "about:blank"))
            request.accept()
        else request.reject()
    }
    function updateDocument() {
        // The envelope is trusted application markup. The body is browser-only
        // sanitized mail, never raw MIME or the QTextDocument/composer HTML.
        const csp = "default-src 'none'; script-src 'none'; style-src 'unsafe-inline'; img-src data:; font-src 'none'; frame-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'"
        const body = root.html.replace(/<img\b[^>]*\bdata-nkmail-src="[^"]*"[^>]*>/gi, function(tag) {
            const alt = /\balt="([^"]*)"/i.exec(tag)
            return alt && alt[1].trim() ? '<span>' + alt[1] + '</span>' : ''
        })
        const document = '<!doctype html><html><head><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="'
            + csp + '"><meta name="viewport" content="width=device-width,initial-scale=1">'
            + '<style>html{background:white;color:#202124}body{margin:0;font:14px/1.55 sans-serif;overflow-wrap:anywhere}img{max-width:100%;height:auto}table{max-width:100%}pre{white-space:pre-wrap}a{color:#1769d2}</style>'
            + '</head><body>' + body + '</body></html>'
        root.documentUrl = documentProfile.setDocument(document)
        root.url = root.documentUrl
    }
    // Measure sender layout with application-owned code in an isolated world;
    // mail scripts stay disabled and no WebChannel/host object is exposed.
    function measureDocument() {
        const expectedUrl = String(root.documentUrl)
        runJavaScript("document.body ? Math.ceil(Math.max(document.documentElement.scrollHeight, document.body.scrollHeight, document.body.getBoundingClientRect().height)) : 0", 1, function(value) {
            if (String(root.url) !== expectedUrl || !(Number(value) > 0)) return
            root.documentHeight = Math.max(200, Math.min(12000, Number(value)))
            root.documentReady = true
        })
    }
    onLoadProgressChanged: if (loadProgress === 100) measureTimer.restart()
    onWidthChanged: measureTimer.restart()
    onHtmlChanged: { documentHeight = 200; documentReady = false; updateTimer.restart() }
    Component.onCompleted: updateTimer.restart()
    Timer { id: updateTimer; interval: 0; onTriggered: root.updateDocument() }
    Timer { id: measureTimer; interval: 50; onTriggered: root.measureDocument() }
}

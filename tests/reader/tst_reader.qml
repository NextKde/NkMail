import QtQuick
import QtTest
import NextKde.Mail.ReaderTests 1.0
import NextKde.Mail.TestViews 1.0 as App

TestCase {
    id: testCase
    name: "MailBrowser"
    when: windowShown
    visible: true
    width: 1000; height: 850
    ReaderTestServer { id: server }
    Component { id: browserComponent; App.MailHtmlBody {} }
    function notification() {
        return '<style>.card{box-sizing:border-box;max-width:600px;margin:0 auto;padding:32px;background:#f5f6f8;border-radius:12px;font-family:sans-serif}.brand{font-size:22px;font-weight:700;color:#1769d2}.title{font-size:26px;line-height:1.3;margin:24px 0 16px}.details{width:100%;border-collapse:collapse;margin:20px 0;background:white}.details td{padding:12px 16px;border-bottom:1px solid #e5e7eb}.muted{color:#687080}.button{display:inline-block;padding:10px 22px;border-radius:6px;background:#1769d2;color:white;text-decoration:none;font-weight:600}</style>'
            + '<div class="card"><div class="brand">Example Account</div><h1 class="title">New sign-in activity</h1>'
            + '<p>We noticed a new sign-in to your account. Here are the details:</p>'
            + '<table class="details"><tr><td class="muted">Device</td><td>Example browser on Linux</td></tr><tr><td class="muted">Location</td><td>Example city</td></tr><tr><td class="muted">Time</td><td>3 November, 09:43</td></tr></table>'
            + '<p>If this was you, no action is needed.</p><a class="button" href="https://example.invalid/activity">Review activity</a>'
            + '<p class="muted" style="font-size:12px;margin:28px 0 0">Synthetic notification. No real account is connected.</p></div>'
    }
    QtObject {
        id: readerMail
        property var account: ({name: "Example Account", address: "reader@example.invalid"})
        property string currentFolder: "INBOX"
        property int blockedImageCount: 0
        property bool remoteImagesLoading: false
        property int failedImageCount: 0
        property bool configured: true
        property bool demo: true
        function clearError() {}
    }
    Component { id: readerComponent; App.MessageReader {} }
    function test_reader_integration_and_plain_text_toggle() {
        const reader = createTemporaryObject(readerComponent, testCase, {x: 20, y: 20, width: 900, height: 800,
            mail: readerMail, hasMessage: true, message: {sender: "Example Account", senderAddress: "notice@example.invalid",
                subject: "New sign-in activity", to: "reader@example.invalid", date: "2026-11-03T09:43:00Z", folder: "INBOX",
                displayHtml: notification(), html: "<p>Plain fallback</p>", text: "Plain fallback", attachments: []}})
        verify(reader !== null)
        const loader = findChild(reader, "browserBodyLoader")
        tryVerify(() => loader.item !== null)
        const view = loader.item
        tryVerify(() => inspect(view, "!!document.querySelector('.card')") === true, 20000)
        tryVerify(() => view.documentReady && view.documentHeight > 300)
        tryVerify(() => loader.height >= view.documentHeight - 1)
        compare(findChild(reader, "messageBody").visible, false)
        if (server.capturePath.length) {
            waitForRendering(reader)
            grabImage(reader).save(server.capturePath)
        }
        reader.showRichText = false
        tryCompare(loader, "active", false)
        tryCompare(findChild(reader, "messageBody"), "visible", true)
        const plainBody = findChild(reader, "messageBody")
        compare(plainBody.getText(0, plainBody.length), "Plain fallback")
    }
    function test_long_message_has_a_visible_draggable_scrollbar() {
        const reader = createTemporaryObject(readerComponent, testCase, {x: 20, y: 20, width: 800, height: 500,
            mail: readerMail, hasMessage: true, message: {sender: "Example sender", subject: "Long synthetic mail",
                displayHtml: '<div>' + Array(100).fill('<p style="margin:12px 0;padding:12px;background:#f2f6fa">Synthetic message section with readable content.</p>').join('') + '</div><p id="end">End of message</p>',
                text: "Long body", attachments: []}})
        verify(reader !== null)
        const loader = findChild(reader, "browserBodyLoader")
        tryVerify(() => loader.item !== null && loader.item.documentReady, 20000)
        const scroll = findChild(reader, "readerScroll")
        const bar = findChild(reader, "readerVerticalScrollBar")
        tryVerify(() => scroll.contentHeight > scroll.height * 5)
        tryVerify(() => loader.height >= loader.item.documentHeight - 1)
        tryVerify(() => bar.visible && bar.size < 1 && bar.width > 0 && bar.height > 100)
        const flick = scroll.contentItem
        mouseWheel(reader, 400, 250, 0, -360)
        tryVerify(() => flick.contentY > 0)
        mouseDrag(bar, bar.width / 2, (bar.position + bar.size / 2) * bar.height, 0, 200)
        tryVerify(() => flick.contentY > scroll.height)
    }
    function test_large_inline_document_and_loaded_image() {
        const view = createTemporaryObject(browserComponent, testCase, {width: 660, height: 500, html:
            '<img id="inline" width="40" height="40" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aX2kAAAAASUVORK5CYII=">'
            + '<p id="large">' + '测试'.repeat(150000) + '</p>'})
        verify(view !== null)
        tryVerify(() => inspect(view, "!!document.getElementById('large')") === true, 20000)
        // The element exists before Chromium has finished parsing its large text node.
        tryVerify(() => inspect(view, "document.getElementById('large').textContent.length") === 300000, 20000)
        tryVerify(() => inspect(view, "document.getElementById('inline').naturalWidth") === 1)
    }
    function inspect(view, code) {
        let result = null, completed = false
        view.runJavaScript(code, 1, function(value) { result = value; completed = true })
        tryVerify(() => completed, 10000)
        return result
    }
    function test_notification_layout_and_isolation() {
        const view = createTemporaryObject(browserComponent, testCase, {width: 660, height: 720, x: 40, y: 40, html: notification()})
        verify(view !== null)
        tryVerify(() => !view.loading && view.loadProgress === 100, 20000)
        tryVerify(() => inspect(view, "!!document.querySelector('.card')") === true, 20000)
        const state = JSON.parse(inspect(view, "JSON.stringify({table:document.querySelectorAll('table tr').length,padding:getComputedStyle(document.querySelector('.card')).paddingTop,size:getComputedStyle(document.querySelector('.title')).fontSize,width:document.documentElement.scrollWidth,height:document.body.scrollHeight})"))
        compare(state.table, 3)
        compare(state.padding, "32px")
        compare(state.size, "26px")
        verify(state.width <= 660)
        verify(state.height > 300 && state.height < 720)
        compare(view.settings.javascriptEnabled, false)
        compare(view.profile.offTheRecord, true)
        compare(view.settings.localContentCanAccessFileUrls, false)
        compare(view.settings.localContentCanAccessRemoteUrls, false)
        view.width = 330
        tryVerify(() => Number(inspect(view, "document.documentElement.scrollWidth")) <= 330)
    }
    function test_active_content_and_navigation_are_blocked() {
        const before = server.hits
        verify(server.url.length > 0)
        const view = createTemporaryObject(browserComponent, testCase, {width: 660, height: 500, html:
            '<p id="content">Safe content</p><script>document.getElementById("content").textContent="executed"</script>'
            + '<img src="' + server.url + '/tracker"><iframe src="file:///etc/passwd"></iframe>'
            + '<style>body{background-image:url(' + server.url + '/background)}</style>'})
        verify(view !== null)
        tryVerify(() => !view.loading && view.loadProgress === 100, 20000)
        tryVerify(() => inspect(view, "!!document.getElementById('content')") === true, 20000)
        compare(inspect(view, "document.getElementById('content').textContent"), "Safe content")
        compare(inspect(view, "Array.from(document.images).filter(i=>i.naturalWidth>0).length"), 0)
        compare(inspect(view, "(()=>{try{return document.querySelector('iframe').contentDocument?.body?.textContent||''}catch(e){return ''}})()"), "")
        const originalUrl = String(view.url)
        view.url = server.url + "/navigation"
        wait(250)
        verify(String(view.url).indexOf("127.0.0.1") < 0)
        compare(server.hits, before)
    }
}

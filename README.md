# NkMail

A native Qt Quick mail client for NextKde. The interface takes cues from macOS Mail, uses canonical **Kos.Ui** from nextkdeAppDemo, and uses **mailio** for IMAP, POP3, SMTP and MIME.

> Under active local development. See [GOAL.md](GOAL.md) for the actual acceptance checklist and [progress](docs/progress/README.md) for verified milestones. A rendered preview is not proof of provider compatibility.

## Install or try the preview

See [installation and first-use instructions](docs/INSTALL.md).
Flatpak packaging shares the KDE 6.11 runtime with kos-apps; see
[build, permissions and release procedure](packaging/flatpak/README.md). Preview releases
provide an Arch Linux x86_64 package, complete source bundle (including Kos.Ui),
checksums and a rebuild recipe. Other Linux distributions require a source build
with Qt 6.8+; no universal Linux binary or Windows/macOS package is provided.

## Current scope

Multiple accounts with password or application-password authentication, folders, paginated headers,
on-demand sanitized reading, opt-in remote images, cached/server search, attachments,
durable drafts, compose/reply/reply-all/forward, delayed SMTP submission and Sent copies.
Flag changes and targeted delete/move commands are supported. Delete/move requires
UIDPLUS; unsupported servers are rejected before any write. A partial server operation
is reported for reconciliation, never automatically retried. No IDLE or background
daemon. Server acceptance is not delivery; uncertain sends remain non-resendable.

Mailio is a protocol/MIME library, not a complete application backend. NkMail supplies the application state, cache, credentials, UI and workflows. The vendored dependency includes the documented [TLS verification and PEEK patch](patches/mailio/0001-verified-tls-peek-timeouts.patch); its loopback acceptance tests do not by themselves prove real-provider compatibility.

## Gmail accounts

Choose **Google / Gmail**, enter your email address, and continue with a Google
**app password** generated after enabling two-step verification. The normal Google
account password is not supported. Some organization/security policies disable app
passwords. See [Google's instructions](https://support.google.com/accounts/answer/185833).
NkMail supports IMAP or POP3 incoming mail with SMTP sending. Gmail requires an app password where permitted by account policy. Remembering credentials uses
the system keychain, with no plaintext fallback.

Browser OAuth sign-in is hidden from account setup. The existing OAuth backend is
retained for compatibility with previously added accounts; new users do not need
Google Cloud configuration. Existing OAuth accounts can switch to an app password
through account settings without removing their cached mail. The earlier
[OAuth distributor notes](docs/GOOGLE_OAUTH.md) are retained for future development.

## Uncached message latency

Header pages and on-demand bodies share a short-lived IMAP connection on the
account synchronization worker. Opening a body after a recent header fetch, or
opening another uncached body, avoids another TLS/authentication exchange. Each
read still uses read-only EXAMINE and UID-based BODY.PEEK; body reads revalidate
UIDVALIDITY before fetching.

Sessions expire after 60 seconds of idle reuse eligibility, with queued idle
cleanup, and are reset on account switching, edits/removal and disconnect.
Credential, endpoint and custom trust-anchor changes prevent reuse. A network
failure on a reused read connection permits one fresh connection attempt; protocol
errors and new-connection failures do not loop. Sending and mailbox mutations do
not use this retry path. Cold connections and downloading large messages still
incur network latency; this change does not prefetch unseen mail.

## Languages

Simplified Chinese and English are included. The interface follows the system language
by default (Chinese → Simplified Chinese, other languages → English). Choose **More →
Language: follow system / 简体中文 / English** to switch without restarting. The selection
is saved in the profile's `language.ini`; isolated demo/screenshot runs keep temporary
preferences and do not modify your real profile. `--language=system|zh_CN|en` selects a
language at startup. A second interactive launch activates the existing instance rather
than changing its language; use its menu to switch.

LanguageService owns loading and preference persistence, QML retranslation and date
locale are wired by the application, and the tray updates on LanguageChange. Each shipped
catalog contains complete UI and application diagnostic translations. Mail contents,
custom server folder names and raw system/provider diagnostics retain their original text.
Existing operation messages may keep the language in which they were generated until
the next status update. Standard Qt dialogs use the installed Qt translation package.

Update catalogs with `lupdate src qml -no-obsolete -ts translations/nkmail_zh_CN.ts
translations/nkmail_en.ts` (Qt's binary directory may need adding to PATH), then translate
new entries in both files. CMake compiles and embeds the QM files. The catalog gate checks
source coverage, finished entries and matching placeholders; never hand-edit generated QM files.

## Single instance

Interactive NkMail windows share one instance per desktop D-Bus session, including
`--demo`. Launching again restores the existing window (also when hidden in the tray)
and exits successfully, before opening any new data directory. A different `--data-dir`
does not create another interactive instance; exit the running client before switching
profiles or between demo and normal mode. Mailbox locking remains a separate storage
safeguard. Screenshot exports bypass desktop ownership and never create a tray icon.
An unavailable session bus is reported as a startup error rather than allowing duplicates.

## System tray

Closing the window saves the open draft and hides NkMail in the system tray; background
work continues. Click the tray icon or choose **显示 NkMail** to restore it. Choose
**退出 NkMail** or press Ctrl+Q to exit. Explicit exit is blocked while a send is pending
or in progress, or when the current draft cannot be saved. Without a supported tray,
closing exits normally with the same protections. If the tray host disappears while
NkMail is hidden, the window is restored. Screenshot mode does not create a tray icon.
This does not install login autostart or a separate background daemon.

## HTML mail rendering

HTML mail uses Qt WebEngine Quick with its own off-the-record profile. A browser-only
sanitized representation retains tables, inline styles and embedded CSS; the existing
simplified HTML remains separate for replies and the plain/rich-text fallback. The
reader disables mail scripts, storage, downloads and navigation; CSP and a request
interceptor reject external resources and local files. Remote images still require the
existing explicit download action. Chromium's process sandbox stays enabled.

Old cached messages stay readable while the selected body is re-read once using IMAP
PEEK to recover layout that the old sanitizer discarded. Offline messages retain their
fallback until connected. The cache stores a format version to avoid repeated upgrades.

Build dependencies include Qt WebEngine Quick (`qt6-webengine` on Arch). Browser tests
also require Xvfb/xauth/Mesa and run as an ordinary user, with the sandbox enabled.

## Module boundaries

| Module | Owns | Must not own |
| --- | --- | --- |
| `TrayService` / `TrayBackend` | Window visibility, guarded exit, tray-host availability / native icon and menu | Mail protocols, draft persistence or send state |
| `MailController` | QML facade, active view generation, result presentation | Protocol calls, SMTP state machine, HTTP downloads |
| `AccountSession` | Profile/account credential keys, session secrets, asynchronous credential resolution | Mailbox/cache contents |
| `MailboxSync` | Serial read-operation scheduling, typed results on the GUI thread | Current QML selection or cache writes |
| `MailboxCommands` | One in-flight write command, confirmed flag/cache updates | Optimistic whole-list rollback or automatic retries |
| `DraftService` | Compose/reply construction, attachments, account-bound draft persistence | SMTP submission |
| `SendService` | Pending-send snapshot/timer, durable send transitions, Sent-copy outcome | Account switching or message selection |
| `RemoteImageLoader` | HTTP limits, in-flight de-duplication, bounded account-scoped rendered cache | Persistent mail storage or protocol worker queue |
| `MailDatabase` | Transactions, account-scoped data, explicit interrupted-send recovery | Network or implicit send recovery in `open()` |

`MailTransport` is the injectable blocking protocol boundary. Services return to their
own QObject thread before touching storage/UI; workers capture immutable inputs and
callbacks use QObject lifetime guards. Account/folder generations reject stale read
results. Account switching is blocked during writes/sending; delayed sending captures
its account and is cancelled on switching. SMTP and IMAP partial failures must not be
reported as rolled back merely because the view did not change.

`Main.qml` composes the shell. `MailSidebar`, `MessagePane`, `MessageReader` and
`MailToolbar` declare their inputs and emit navigation/dialog requests. `MailSelection`
owns row/thread selection; `MailLabels` centralizes presentation helpers. Components
must load without reaching into Main's private IDs. The isolated-view test enforces this.

For a new feature, extend the responsible service and inject a transport in its failure
regressions. Wire the result through the facade and an explicit component property or
signal. Do not add protocol work to QML, duplicate single/batch command paths, or split
one shared mutable controller across multiple implementation files.

## Dependencies

- CMake 3.30+, C++20 compiler, Ninja (optional)
- Qt 6.8+: Core, Gui, Widgets, Qml, Quick, QuickControls2, QuickDialogs2, Network, Concurrent, Sql, LinguistTools, WebEngineQuick; QtTest/QuickTest for testing
- Qt translation catalogs (for standard dialogs, e.g. `qt6-translations` on Arch)
- Qt graphical-effects compatibility module and SVG support as required by Kos.Ui
- QtKeychain for Qt 6; system Secret Service or equivalent
- Boost development files (`date_time`, `regex`) and OpenSSL
- Pinned Kos.Ui source submodule at `third_party/nextkdeAppDemo/shared/qml`

Mailio 0.26.1 source commit: `f358b1f85f9a021f0bd814e285eb8ae102fc8f79`.
Kos.Ui is pinned by the existing gitlink and `third_party/kos-ui.lock.json` to
`e2592b2742c58c1bd797a8bcfa2a3ae7da4affd8`. The current lock is commit-only; it does not
claim a release tag. This revision was inherited from main, not upgraded by the modularity refactor.
System dependencies above must still be installed.

## Clone and build

Contributors in the maintainer workspace run validation in the designated VM.
Users may build and run on their own compatible Linux desktop; see the installation guide.

```sh
git clone --recurse-submodules https://github.com/NextKde/NkMail.git
cd NkMail
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
./build/nkmail --demo
```

For an existing or non-recursive checkout, first run:

```sh
git submodule update --init --recursive
```

GitHub's automatic source ZIP/tarball does **not** include submodule sources. Use a
recursive Git clone. CMake does not download missing dependencies or fall back to a
sibling checkout automatically. Kos.Ui is built into the app; its source checkout is
not needed to run the resulting binary.

### Local UI development and upgrades

To work on shared components, explicitly override the source in a separate build:

```sh
cmake -S . -B build/ui-dev -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DNKMAIL_UI_SOURCE_DIR=/absolute/path/to/nextkdeAppDemo
cmake --build build/ui-dev --parallel 4
```

An existing CMake cache may still point to the old sibling workspace, even after this
checkout is updated. Use a fresh build directory for pinned release builds, or pass
`-DNKMAIL_UI_SOURCE_DIR="$PWD/third_party/nextkdeAppDemo"` to explicitly reset the cache.
Development overrides are not proof of acceptance of the pinned version.

Shared fixes belong in the canonical repository. After a new source release, fetch
its tag in the submodule, check out that chosen release, run NkMail's regressions, then
commit the gitlink and version-record updates. Do not use `git submodule update --remote`,
follow `main` or automatically select the newest release. Published tags are immutable;
other applications upgrade on their own schedule.

### Required UI dependency gate

Before opening an integration PR, stage the intended gitlink and lock-file updates,
then run:

```sh
python3 -m unittest discover -s tests/dependency -v
python3 scripts/check_ui_dependency.py
```

`third_party/kos-ui.lock.json` records the pinned full SHA, and may optionally also
record a `tag`. The guard compares the SHA with the staged gitlink and clean submodule
HEAD, requires the canonical URL and no tracking branch, and verifies that the pinned
commit actually exists upstream. If a `tag` is present it must be a concrete version
such as `v0.1.0-rc.1` and must resolve to the same commit (annotated tags included);
a GitHub Release is no longer required. Dirty submodules, mismatched commits and
API/network errors fail the check. The script is read-only and never submits or
publishes shared changes for you.

For a new shared component, the order is: develop/test upstream → commit and push it
upstream → check out that commit in NkMail's submodule → update/stage the gitlink and
lock → run the guard → open a PR. Running `git add` in NkMail does **not** commit files
inside its submodule. For example, after upstream pushes the commit you want to pin:

```sh
git -C third_party/nextkdeAppDemo fetch origin
git -C third_party/nextkdeAppDemo checkout --detach <commit>
git -C third_party/nextkdeAppDemo rev-parse HEAD
# Put that full SHA in third_party/kos-ui.lock.json as {"sha": "<full SHA>"}; update README/NOTICE.
git add third_party/nextkdeAppDemo third_party/kos-ui.lock.json README.md NOTICE.md
python3 scripts/check_ui_dependency.py
```

The checker deliberately accepts a staged dependency upgrade before the parent commit,
so you can fix a missing upstream commit before opening the PR. It does not need a
locally installed Git hook, and does not silently repair the dependency.

Every PR to `main` runs the `release-gate` GitHub Actions job: guard regressions,
release verification, a fresh default-source Release build and all five CTest suites.
The main-branch policy requires a PR and that check against the latest base, including
for administrators, with no force-push/deletion. No second-person approval is required
for this single-maintainer workflow. Repository administrators can still change the
protection settings; this prevents accidental omissions, not malicious policy edits.

CI cannot inspect another local UI workspace. New/dynamically loaded UI must have a
loading/behavior test; otherwise passing tests cannot prove an intended visual change
was included. An explicit local source override is never the release-gate build.
See [testing and guard boundaries](docs/TESTING.md#ui-release-gate).

## Preview and data

Use `./build/nkmail --data-dir /a/private/directory` for a separate persistent mailbox. Preview mode always uses a fresh temporary directory, never existing credentials, and never the network.

```sh
./build/nkmail --demo --appearance=light --screenshot=tmp/inbox-light.png
./build/nkmail --demo --appearance=dark --review-scenario=compose --screenshot=tmp/compose-dark.png
```

Screenshot export is intentionally blocked outside synthetic preview mode. Public progress images must never expose a personal account, real mail or credentials. Test SMTP services bind only loopback and do not relay.

## Regression tests (virtual machine only)

Tests additionally require Qt Test / QuickTest, Xvfb/xauth/Mesa, Python 3, `dbus-daemon`, `dbus-send` and the `openssl` executable (for short-lived loopback certificates). They do not need a real account. Core/controller tests use temporary data directories; protocol services bind only `127.0.0.1`; UI tests run offscreen with synthetic state; browser tests use an Xvfb OpenGL surface as an ordinary user with the Chromium sandbox enabled.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNKMAIL_BUILD_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

For visual review, run the actual app rather than treating offscreen tests as a GPU/material check. See the [testing guide](docs/TESTING.md) for suite boundaries and manual acceptance, and the [progress reports](docs/progress/README.md) for exact verified results and known gaps.

## Privacy and safety

Passwords belong in the system keychain, never in settings, SQLite, source control or logs. A locked/unavailable keychain falls back to the current session only, not a plaintext password file. Cached messages are local sensitive data and are **not encrypted at rest**; protect the data directory accordingly. Remote images are blocked until explicitly requested; downloaded images are cached only in bounded session memory. Attachments are never automatically executed or opened.

Do not use a primary personal mailbox until the documented protocol and TLS acceptance checks are complete. Real-provider verification and external delivery are separate from local loopback tests.

## Design references

- [Apple Mail user guide](https://support.apple.com/guide/mail/welcome/mac): restrained message hierarchy, compact toolbar groups and a spacious reading pane. Reference artwork is not redistributed.
- Local KMusic: application bootstrap, right-hand window controls and controller/model patterns.
- Local calendar: anchored content popovers, modal confirmations and fixed editor footers.

## License

GPL-3.0-or-later for NkMail. See LICENSE and NOTICE.md. Third-party components retain their own licenses.

### Cooperative window controls

`MailWindowControls.qml` adapts the pinned Kos.Ui controls to the optional installed
`Kos.SurfaceShape.WindowControlSlot` platform module. It sends the actual delegate
circle rectangles, preserves layout, and hides/disables client controls only while
the adapter reports compositor takeover. Missing modules/protocol support retain
client drawing; no Wayland dependency is added to Kos.Ui. The compatibility mapping
uses the pinned component's action object names and is covered by UI regression tests.
Calendar geometry is used: 14px circles in 22px button cells, 6px spacing, with the
row 10.5px from the right and 12.5px from the top (4px below Calendar), centred in a 47px header. Native takeover requires the v2
NextKde window-controls protocol and enabled KOS window appearance. The older pinned
fallback keeps its existing hover-glyph preference; global fallback glyph preference
integration awaits a tested canonical UI release exposing that property.

### Remote image preferences

Remote images are blocked by default. “Show images for this message” remembers consent for that account/folder/UIDVALIDITY/UID across restarts. “Allow images from this sender” remembers the exact sender address within the current account. Display options also provide an optional global “Always show remote images” switch, a current-sender toggle and “Reset image permissions”. Removing an account removes its message/sender permissions. Disabling a policy prevents future automatic downloads; images already loaded can remain visible.

Permissions persist locally; downloaded image bodies still use a bounded in-memory cache and may be fetched again after restarting. Automatic loading uses the same image limits, TLS checks, cookie-free requests and retry UI as explicit loading. No server-side mailbox changes are made by image preferences.

### Embedded message images

CID-referenced raster images inside MIME messages render without remote-image permission or network access. PNG, JPEG, GIF and WebP parts are matched by Content-ID (including URL-encoded CID references), checked for valid decoding and capped at 2 MiB / 16 million pixels each. Inline expansion is capped at 8 MiB per rendered body. SVG, broken/missing/ambiguous references and oversized resources fall back to alternative text. Remote HTTP images remain permission-gated. Inline image parts do not clutter the attachment list; explicit attachments remain available.

Body cache format 2 retains the resolved images in sanitized HTML, making them available offline. Older selected cached messages are upgraded with a read-only fetch when credentials are available; their existing content stays visible while upgrading.

### Unified inbox (entry temporarily hidden)

The smart-mailbox sidebar section is currently hidden at the maintainer's request. Hierarchy, scope and configurable smart-mailbox rules are deferred in GOAL.md; the implementation below is retained for future use. Current-account shortcuts follow the active account; personal favorites retain their own source account.

**All inboxes** combines cached inbox headers across saved accounts, retaining each row's account/folder/UIDVALIDITY/UID identity. The list shows the source account and opens content with that account's credentials. Search and unread filtering operate on the local cache; Refresh independently downloads the newest 50 inbox headers from each saved IMAP/POP account with available credentials. Periodic refresh also updates inactive accounts; one account failure does not stop the others. Older messages already cached remain available, and refreshing preserves the loaded aggregate window and current body. This is bounded recent-header synchronization, not a full-history mirror or server-deletion reconciliation. Older uncached mail can be fetched from its source account folder. The aggregate supports read/unread, flags, delete and archive, routed to each message's source account and mailbox epoch. Mixed-account selections are processed per account; partial failures are reported and never automatically retried. Archive uses each account's own cached archive destination. Move is available for a selection from one account and uses that account's folder list; cross-account moves are not offered. POP read/flag/delete remain local, and POP move/archive remain unavailable. Opening an unread row marks it read independently of the PEEK body fetch. Replies use the selected message's source account.

### POP accounts

Choose **POP** in the incoming-protocol selector when adding an account. Default security is implicit TLS (995); STARTTLS uses port 110 and never falls back to plaintext. The server must provide stable UIDL identifiers. Header pages use TOP, with a bounded RETR fallback when TOP is unavailable. Bodies and attachments use the same sanitized MIME reader as IMAP.

POP keeps all server copies: NkMail never sends DELE. Read/star state and **Delete locally** belong to this device; deletion tombstones prevent a refresh from restoring locally deleted mail. Cached messages remain available when server copies disappear, though an uncached body cannot be recovered after server removal. SMTP-accepted mail receives a local Sent copy; ambiguous sends are never retried. Folder moves/archive and server search are unavailable, and unread state belongs to this device. Background inbox receipt and new-mail notifications support both active and inactive POP accounts. Switching incoming protocol on an existing account requires re-adding it.

The add-account list shows supported password/app-password providers and a manual IMAP/POP + SMTP option. Microsoft/Exchange is absent because its required authentication is not implemented; hiding it does not mean Microsoft lacks standard mail protocols. iCloud offers IMAP only. Provider presets are not a claim of live-account certification; verification checks both incoming and outgoing servers without sending mail.

### First mail check after account setup

After verification and saving a new account, NkMail automatically discovers its
folders and downloads the newest inbox headers. The completion page shows live
progress, success or the actual failure; **Retry mail check** reuses the saved
account instead of creating another account. **Open inbox** navigates to the new
account. Closing the dialog leaves receipt running; a failed check also has a
retry action in the main window notice. Adding a second account does not replace
the current reader. Bodies remain on demand, and an empty inbox is a successful
check. Verification and receipt do not submit SMTP mail.

### New-mail notifications and unread tray count

While NkMail is running (including hidden in the tray), the refresh timer checks
all saved accounts' inboxes independently of the folder being viewed. The default
interval remains five minutes; this is polling, not IMAP IDLE. Both IMAP and POP
use the same post-cache notification path. The first successful observation and
account/UIDVALIDITY changes establish a quiet baseline; historical mail is not
announced as new. IMAP cursors and opaque POP UIDL identities persist per profile
and account. Turning notifications off still advances these cursors, so turning
them back on does not replay old arrivals. Receipt retains the bounded newest
50-header synchronization; notification summaries concern newly fetched unread
headers, not a full-history reconciliation.

**Display → New mail notifications** is saved in the profile's `notifications.ini`.
Desktop notifications carry an Open message action bound to account, endpoint,
folder, UIDVALIDITY and UID. Clicking restores the window and opens that exact
cached header, fetching its body if necessary. Active writes/sends finish before
navigation; stale/deleted-account targets fail safely. The desktop must support
notification actions, and the application must still be running. Notification
transport is asynchronous and tolerates a missing/restarted notification daemon.

The tray badge shows the unread total across **locally synced inboxes**, not an
estimate of all unseen server mail. It updates on receipt, read/unread changes,
local deletion and account removal. Counts above 99 display `99+`; the tooltip
contains the full count. Zero clears the badge. Popup notification settings do
not hide the unread badge. No background daemon or automatic startup is added.

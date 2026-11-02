# NkMail local delivery goal

## Inbox top separator · 2026-11-02

- Suppressed the first message row top separator so it does not overlap the inbox header rule. Inter-row separators retain their existing appearance. VM UI regression passed in 11.21s; host build and guarded preview replacement succeeded.

## Search spacing refinement · 2026-11-02

- Increased search-to-traffic-light spacing from 5px to 10px for both icon and input. VM UI regression passed in 11.32s, with half-pixel layout-rounding tolerance. Host build and guarded preview replacement succeeded.

## Search beside traffic lights · 2026-11-02

- Removed cumulative toolbar/search trailing margins; search now ends at most 5px before the visible first traffic-light circle. Moved compact-search close action inside the input with reserved text padding so the input itself reaches this edge.
- VM UI passed in 11.36s, covering icon/expanded/wide input at four widths. RowLayout pixel rounding makes the gap slightly under 5px; assertions enforce the requested maximum rather than exact fractional placement. Host build and guarded preview replacement succeeded.

## Header layout and four-pixel offset · 2026-11-02

- Set the control centre to 23.5px, 4px below its original position, and centre all titles/controls within a 47px header. List, reader, sidebar and dividers follow the header bottom. Replaced rounded centre anchors with exact centre expressions after VM assertions exposed a half-pixel mismatch.
- VM UI regression passed in 10.44s, including title/control centre alignment and content geometry. Host build and guarded preview replacement succeeded.

## Refined header offset · 2026-11-02

- Reduced requested downward offset from 10px to 6px (4px upward from previous preview), retaining the common alignment. VM UI regression passed in 10.32s; host build and guarded preview replacement succeeded.

## Header controls vertical offset · 2026-11-02

- Moved traffic-light row down 10px. Existing shared center-line bindings move toolbar groups, search, filter, folder title and sidebar toggle together; header height remains 44px. Updated expected circle Y coordinate to 22.5px.
- Host build and guarded preview replacement succeeded. VM UI regression passed in 10.59s.

## Cooperative window controls · 2026-11-02

- Added an app-owned optional platform adapter for the pinned Kos.Ui controls, using actual delegate rectangles and `WindowControlSlot`. Native takeover hides/disables fallback only after active acknowledgement; loss restores it. No reference project or dependency pin changed. Calendar placement and 22px client hit cells now match.
- VM build and UI regression passed (10.44s), including exact circle coordinates, native-active simulation, restoration and missing-adapter fallback. No host validation performed.
- Attempted real VM Wayland capture: no window-controls protocol appeared in the trace, bridge debug returned no diagnostics, and capture failed because the window was not exposed. Real compositor/GPU acceptance remains unverified; no desktop configuration was changed. The existing host preview still runs the previous binary.

## Preserve inbox header during search · 2026-11-02

- Removed search-state coupling from the message-list header visibility. Opening search only replaces reader-toolbar actions; the mailbox title and unread filter remain visible.
- Added assertions for title/filter visibility during compact search. VM UI regression passed in 9.98s; host build and guarded single-instance preview replacement succeeded.

## Consistent toolbar sizing and compact search · 2026-11-02

- Unified filter, sidebar, action groups and search to the shared 28px toolbar height. Visible groups retain horizontal labels; operations that do not fit move to existing menu commands. Search/back controls no longer use stacked labels. Compact options retain a View route through More.
- Compact search now exposes a visible input and close control; header actions give it room. Escape prioritizes active search over sidebar collapse. Updated geometry assertions and exercised compact typing/escape. Initial regressions exposed the old Escape priority; fixed without weakening the test.
- VM UI/catalog checks passed 2/2 in 10.12s. Host build succeeded and old preview exited through guarded tray quit before replacement; single instance retained.

## Top-only message separators · 2026-11-02

- Moved each message separator to the top of its row, leaving no trailing line below the final message. Preserved horizontal insets, lighter opacity and selected-row treatment.
- Host build and guarded single-instance preview replacement succeeded. VM UI regression passed in 9.67s.

## Joined toolbar hover surfaces · 2026-11-02

- Replaced independent capsule hover backgrounds inside toolbar groups with clipped segment highlights: rounded only at the two outside ends, square at internal boundaries, explicitly antialiased. Preserved the canonical shared liquid surface and keyboard focus ring. Rounded segment widths to whole pixels and disabled per-cell scaling.
- VM UI/catalog regressions passed 2/2 in 9.87s. A temporary offscreen hover capture exercised first/middle/last segments; inspected the captured hover geometry (software rendering, not GPU acceptance). Initial capture harness lacked visible:true and incorrectly treated save() as returning a bool; corrected the harness and reran successfully.
- Host build succeeded and the prior preview exited through guarded tray quit before replacement; single-instance preview retained.

## Softer row edges · 2026-11-02

- Reduced message selection/hover corner radius to 6px. Separators now render at half their existing theme opacity, preserving light/dark colors and horizontal insets.
- Host build and guarded single-instance preview replacement succeeded. Final VM UI regression passed in 9.48s.

## Compact inset message rows · 2026-11-02

- Reduced message row height from 84 to 68 and vertical content padding to 7px, with a single elided summary line. Text and separators now have 24px left/right insets. Selection/hover surfaces have 8px horizontal insets and 8px corners; active white text is preserved.
- VM UI and translation guards passed 2/2 in 9.40s; inspected the 1180×760 Chinese synthetic screenshot. Host build succeeded and the prior preview exited through the guarded tray menu before replacement.

## Chinese/English localization · 2026-11-02

- Added LanguageService with profile-scoped language preference, system default, English/zh_CN selection, embedded Qt catalogs, live QQmlEngine retranslation and optional installed Qt dialog catalogs. More exposes all three choices; --language supports isolated reviews. Demo preferences remain temporary.
- Completed 353 entries in each catalog, covering QML, tray, account/draft/send status and application-owned protocol/storage errors. Restored extraction for SendService callbacks. Standard folders and dates follow the selected locale; user mail/custom server text is not translated. Already-generated operation messages can retain their original language until a new status update.
- VM full seven-suite regression passed in 45.21s before the catalog guard/native-tray test extension; final i18n + source/catalog guard passed 2/2 in 0.15s, including live native tray-menu translation, QML retranslation and saved choice. There are now eight suites required by CI. English and Chinese 1180×760 synthetic screenshots were inspected; no GPU-material claim.
- Host build succeeded and the prior preview exited through its guarded tray menu before opening the new Chinese preview. Single interactive instance retained; no real mailbox or reference project was used. No host automated tests.

## Header alignment and spacing · 2026-11-02

- Aligned the unread filter to the shared window-control/toolbar center instead of its independent 44px row. Reduced shell header height from 68 to 44, removing 24px of unused space above the list and bringing all panes upward together.
- VM UI regression passed in 9.69s, including an explicit filter/toolbar center alignment assertion and updated header geometry checks. Inspected the 1180×760 synthetic screenshot. Host build succeeded; replaced the previous preview via guarded tray exit, retaining one instance.

## Smaller default window · 2026-11-02

- Reduced the initial window from 1280×820 to 1180×760, retaining the compact 189px sidebar and 280px message list. Saved user window sizes remain respected.
- Host build succeeded; replaced the previous synthetic preview through guarded tray exit and opened at defaults. VM UI regression passed in 9.38s.

## Compact initial message list · 2026-11-02

- Reduced the message list default/reset width from 320 to 280; retained drag resizing, the 260 minimum and saved custom widths. Updated keyboard/reset geometry expectations.
- Host build succeeded; previous preview exited through its guarded tray menu before replacement. VM UI regression passed in 9.43s.

## Narrower navigation sidebar · 2026-11-02

- Reduced the navigation sidebar default/reset width from 236 to 189 (approximately 20%) and its minimum from 190 to 152. Explicit saved custom widths remain respected; list/reader widths are unchanged.
- Host build succeeded and the previous synthetic preview exited via its guarded tray menu before replacement. VM UI regression passed in 9.49s, including updated drag and geometry expectations.

## Active message contrast · 2026-11-02

- Current message rows now use solid accent blue with white sender, subject, preview, date, unread marker and status icons. Thread badges use a translucent white surface; inactive and multi-selection styles are preserved.
- VM UI regression passed in 9.39s and the synthetic screenshot was inspected. Host build succeeded; the prior preview exited through its guarded tray menu before the replacement opened, preserving one interactive instance.

## Desktop single-instance correction · 2026-11-02

- Root cause: only data-directory locking existed; each interactive demo used a fresh temporary directory and therefore could create another window/tray. Added an independent SingleInstance D-Bus service before storage/controller/tray construction. Repeated interactive starts activate the existing window and exit, regardless of demo/profile arguments. Profile locking remains. Screenshot exports stay isolated with no tray.
- VM process/tray regression passed **2/2 in 9.83s**, covering normal/profile relaunch, demo relaunch, ownership release/restart and screenshot export alongside a running demo. Tests run on a private session bus; CI explicitly installs D-Bus tools. Host build succeeded.
- Removed only the duplicate synthetic host previews started in this chat and replaced the final old preview with the corrected binary. Read-only host process/watcher inspection confirmed **one NkMail process and one registered tray item**. Runtime log was empty. No real mailbox was accessed.

## Remove permanent bottom bar · 2026-11-02

- Removed the entire status strip and extended sidebar, content and column dividers to the window bottom. Theme switching moved to More. Errors, blocked-exit notices and active live-account work use an on-demand floating notice without reserving footer space.
- Host build and synthetic preview launch succeeded. VM UI regression passed in 9.40s; geometry checks now assert full-height panes/dividers and absence of statusStrip. Corrected one stale status.y test reference found on the first run.

## Quiet preview status · 2026-11-02

- Removed the permanent Demo/synthetic-mail status caption and its idle dot from the preview footer. Lifecycle notices and errors remain visible; normal account status behavior is unchanged. Preview mode still uses synthetic data and disables sending.
- Host build and preview launch succeeded; VM UI regression passed in 9.54s.

## Shared sidebar glyph and compose placement · 2026-11-02

- Replaced panelLeft with canonical KosSidebarToggle (rounded outline and filled inset pane, as used by Weather/Calendar), retaining the liquid background. Removed the sidebar-header compose button; main-toolbar compose remains. Recorded the user preference in AGENTS.md.
- Host build succeeded and synthetic preview opened. VM UI regression passed in 9.52s, including pointer activation of the remaining compose action and absence of the removed sidebar button. Initial test lookup of a Repeater delegate returned null; corrected the test to click its stable group coordinates, without bypassing pointer handling.

## Icon-only view toggles · 2026-11-02

- Changed the unread filter and sidebar toggle to compact icon-only liquid buttons. Kept accessible names, hover hints, filter selection state and existing actions.
- Host build succeeded and a new synthetic preview was opened. VM UI regression passed in 9.62s; no host automated tests.

## Grouped inline liquid toolbar · 2026-11-02

- Reduced labels from 12px to 11px and icons from 15px to 14px. Each action group now shares one canonical liquid surface, with transparent interactive cells and subtle short separators; horizontal labels, shortcuts/tooltips and action routing are preserved.
- Host build succeeded and the updated synthetic preview was opened. VM UI regression passed in 9.85s; inspected the 1600px software preview for grouping/layout (not GPU material acceptance).

## Restore inline liquid controls · 2026-11-02

- Restored canonical LiquidActionButton glass material, capsule geometry and theme ink for inline toolbar actions. Each group now receives the shell backdrop and material preference; horizontal icon/label layout remains unchanged. No shared dependency source was changed.
- Host build succeeded and a new synthetic preview was opened. VM UI regression passed in 9.32s. No host automated tests or real-mail access.

## Inline toolbar refinement · 2026-11-02

- Replaced stacked capsule groups with a mail-specific horizontal composition of canonical Kos.Ui buttons: 15px icons, 12px labels, 30px hit targets, hover surfaces and group whitespace. Narrow layouts retain icons and accessible tooltips. Shared dependency sources remain untouched.
- Fixed compact More action routing while preserving action indices and object names. VM UI regression passed in 9.46s; inspected a 1600px software-rendered synthetic preview. Host preview build uses the previously authorized local preview workflow; automated tests remain VM-only.

## System-tray persistence · 2026-11-02

- User explicitly authorized implementing systray persistence, compiling and opening NkMail on the host for this task. Automated regressions remain VM-only; reference projects are untouched.
- Added independent TrayService lifecycle policy and native TrayBackend implementation. Window close saves the draft and hides into the tray; activation restores; menu/Ctrl+Q exits through a draft/send guard. Missing/disappearing tray hosts cannot strand a hidden client. No login autostart was added.
- Default pinned-source Debug builds succeeded on host and VM. VM CTest passed **6/6 in 39.61s**, including new tray lifecycle tests and QML close/send/draft protection regressions. No real account or mailbox writes were used.
- Opened host `build/host-tray/nkmail --demo --appearance=light`; process remained running and its StatusNotifierItem was registered with the desktop watcher. Runtime log was empty. Host automated tests were not run.
- The prior modularity commit fb2749c was published to PR #5; main remains protected and unmerged.

## Modularity implementation · 2026-11-02

- Authorized scope: repair the review findings and split maintainable feature modules. Branch `codex/modular-mail-services`; all builds, execution and tests are restricted to the designated VM. No real account, external SMTP, reference-project modifications, main push or dependency upgrade.
- Added AccountSession, MailboxSync, MailboxCommands, DraftService, SendService and RemoteImageLoader with an injectable MailTransport boundary. Removed duplicated single/batch controller operations; extracted sidebar/list/reader/toolbar/selection/labels QML with explicit inputs/signals and independent loading tests.
- Safety repairs: no whole-mailbox EXPUNGE fallback; UIDPLUS checked before any delete/move write on one session; partial failures are explicit and never automatically retried. Interrupted-send recovery is startup-only. Network pages update active UIDVALIDITY. Confirmed flag updates persist summary and body together. Sidebar reads use scoped SELECTs without rebinding. Background accounts resolve credentials securely; disconnect restore permission survives restart. Draft account ownership is enforced in service and storage. Image cache keys include account identity, store rendered HTML in bounded session memory and support cancellation.
- VM regression milestones: complete five-suite run passed after initial extraction (38.30s); another complete run including disconnect persistence passed (37.96s). New protocol fixture initially lacked mandatory RECENT response; corrected the fixture. Strict QML tests caught a stale search-control ID and incomplete mock interface after toolbar extraction; fixed both without weakening warnings. Final VM Debug/default-source build and all five CTest suites passed in **38.80s** on Ubuntu 26.04 / Qt 6.10.2: UI 20, core 19 and controller 33 QtTest passes (including init/cleanup), plus CLI acceptance and 37 loopback protocol/TLS scenarios. The final command-service input guards and action-time toolbar row lookup are covered.
- VM dependency guard regressions passed **32/32**; VM whitespace/diff check passed. The live dependency guard passed for the unchanged clean canonical gitlink `e2592b2742c58c1bd797a8bcfa2a3ae7da4affd8`, verifying upstream commit existence. This is the inherited commit-only guard, not a published-release acceptance claim. README/NOTICE now accurately identify that existing pin.
- Synthetic light inbox and dark compose captures were generated in the VM and visually inspected (`tmp/modular-review/` locally). The VM Wayland attempt reported an unexposed window; the inspected captures use offscreen/software rendering and are not GPU/material acceptance. No real mailbox/provider verification was performed.
- Ready for review on the local refactoring branch. No remote publication or main merge was performed. Module contracts and regression boundaries are documented in README and docs/TESTING.md.

## Static architecture review · 2026-11-02

- User requested pulling the online NkMail source and reviewing modularity and maintainability. Fast-forwarded local `main` to `fd21cd3da37eea6aa1d6dc0ad4fc1516ef99382e` and initialized the pinned UI submodule. Review scope is merged `origin/main`, not unmerged feature branches. Existing local feature branches were preserved.
- Static inspection only: no compilation, application launch, tests, real-account access or mailbox operations. No implementation fixes, commits or pushes were made in this review.
- High-priority findings: targeted delete/move falls back to whole-mailbox EXPUNGE without UIDPLUS; database `open()` performs interrupted-send recovery even for sidebar probes; background polling receives database accounts whose passwords are explicitly cleared; successful network pages do not update the controller UIDVALIDITY used by batch commands/search. Single/batch flag writes update the model but not persisted cache. Remote-image completion guards outlive the rendered content and omit account identity, preventing subsequent loads after reopening a message or switching to matching IDs in another account.
- Architecture: `MailController.cpp` (1599 lines) mixes account/credential lifecycle, polling, caching, reading, image HTTP, composition, sending and mailbox mutations; `Main.qml` (1658 lines) mixes shell, navigation, selection, toolbar, list and reader. Existing controller tests replace concrete static backend methods at link time; newly added mutation/statistics stubs return empty/success results, leaving these failure paths insufficiently exercised.
- Recommended sequence: fix destructive-operation and send-state issues first with targeted VM regressions; introduce injected mail transport and account-scoped operation context; extract AccountSession, MailboxSync, MailboxCommands, Draft/Send services and RemoteImageLoader with explicit ownership; split sidebar/list/reader/toolbar QML into components with declared properties/signals; separate connection opening, schema migration and one-time interrupted-send recovery. Keep MailController as a thin QML facade. Avoid merely splitting member functions across source files while retaining shared mutable state.
- Documentation drift remains: AGENTS.md/older ledger and account-dialog copy describe read-only/single-account behavior while merged source supports multi-account and server mutations. Reconcile these contracts before release. Runtime acceptance of this revision remains unverified.

## Historical first-release contract

The 2026-11-02 scope and VM-only execution rule above supersede the single-account/read-only scope and local-execution permission below. Historical acceptance results are retained as a recovery record.
Build a working single-account Qt Quick mail client in this directory. Reuse canonical Kos.Ui from the pinned nextkdeAppDemo Git submodule at third_party/nextkdeAppDemo; use mailio for IMAP/SMTP/MIME. External UI workspaces require an explicit NKMAIL_UI_SOURCE_DIR development override. Follow KMusic application patterns and calendar's dialog/popover interactions, with macOS Mail as a visual reference. No server-side flag editing, move or delete in the first release.

The user authorized local builds, GUI and isolated automated testing, installation of the missing Boost development package, and milestone commits/pushes with synthetic screenshots to the public NextKde/NkMail repository. A specific enterprise test account was subsequently supplied for local read-only provider verification, after TLS/PEEK acceptance. Its details and credentials must never enter this repository or public output. No existing mail-client access, external sending, mailbox mutation, default-client registration or reference-project modification.

This file is a recoverable work ledger, not a native Codex Goal. Work requires an active session. The session-only progress timer cannot run after the session exits.

## Acceptance checklist
- [x] Independent CMake build with canonical Kos.Ui and pinned mailio.
- [x] Reviewed TLS verification and side-effect-free PEEK patch, with negative tests.
- [x] Actual loopback IMAP/SMTP integration and MIME/attachment tests.
- [ ] Single account, secure credentials, folders, paginated summaries and on-demand content: implemented; controller/provider integration still to validate.
- [x] Offline cache and cached-only search; UIDVALIDITY-safe identity (core regression).
- [x] Compose/reply/reply-all/forward, attachment copy/save/discard and restart-safe drafts: actual controller integration regression. Unsupported complex recipient formats fail closed for automatic replies; manual composition remains available.
- [x] Send accepted/rejected/ambiguous states, no automatic retransmission (protocol and persistence regression).
- [x] Three-pane UI, dark/light/narrow layouts, canonical dialogs/popovers and keyboard use: rendered, behavior-tested, resizable columns and per-profile geometry persistence; actual two-start window restore checked.
- [x] Automated behavior regression plus real GPU screenshots using synthetic data.
- [x] Public milestone reports and six synthetic screenshots published to main after staged privacy checks.

## Verified facts · 2026-10-29
- Public repository cloned; main created locally; GitHub access and commit identity available.
- Authorized Boost development installation completed successfully.
- mailio pinned to 0.26.1, commit `f358b1f85f9a021f0bd814e285eb8ae102fc8f79`; complete local security/read-only patch retained in patches/mailio/ and reverse-apply checked against vendor source.
- Kos.Ui sibling source HEAD: `54fd5f14a5bc50132d524d053b373b1dc1189935`, with existing local component changes. Those changes are consumed as-is, not altered or copied; HEAD alone does not reproduce this workspace.
- Actual application and test builds succeeded on Qt 6.12.0. Final integrated CTest: **3/3 passed**, 20.46 seconds; core QtTest 16 pass, UI QuickTest 9 pass (including init/cleanup), protocol fixture 30 scenarios.
- Actual GPU app screenshots checked: inbox light/dark, compose, account, details, narrow. All synthetic, sending disabled. Curated artifacts in docs/progress/.
- Fixed initial read-only TextEdit implicitHeight QML error and preview theme initialization. Final UI tests have no QML warnings; upstream C++ and CMake tooling warnings remain.
- Draft state transitions use authoritative database CAS, interrupted sending becomes unknown on restart, and data-directory QLockFile prevents concurrent app instances from reinterpreting a live send.
- First public milestone successfully pushed: `4e4dfea7b51690e01b7ecf8be3c823c0b079529b`, including six reviewed synthetic screenshots.
- A single controlled read-only provider attempt failed before the header/content slice completed; no alternate password was tried. A separate no-login connection verified system CA/hostname TLS and an IMAP greeting, but fixed backend diagnostics do not identify the failed authenticated phase, so this is not proof of an authentication failure. Provider compatibility remains unverified. No real SMTP submission or mailbox mutation has occurred.

## Remaining work / known limits
- Controlled provider read-only verification after the now-passing TLS/PEEK suite; never publish account data or raw output, never cycle password alternatives automatically.
- Controller-level workflow regression added: 23 passed including init/cleanup; reproduced and fixed offline paging, UIDVALIDITY append reset, stale attachments, visible cache on transport change, profile keychain isolation and reply recipient handling.
- Column resizing, window geometry persistence and keyboard/focus checks implemented; pointer-test scene-coordinate issue fixed with unchanged strict assertions, three consecutive UI runs passed.
- Additional backend-only defensive review was interrupted before handback; it is not a completed audit or acceptance result.
- No server Sent APPEND; no remote flags/move/delete/OAuth/IDLE/multiaccount.
- Folder statistics currently use sequential STATUS; network deadlines are per-I/O, not whole-operation limits. MIME tree bounds are checked after mailio parsing, not a complete upstream parser audit.

## Required UI integration gate · 2026-10-31

- Added a read-only release/dependency guard with an explicit tag/SHA lock and 28 offline negative/positive regression tests (all passed locally), including hidden assume-unchanged/skip-worktree index flags. Live verification of the existing canonical prerelease passed.
- [PR #1](https://github.com/NextKde/NkMail/pull/1) bootstraps the hosted gate. [Initial hosted run](https://github.com/NextKde/NkMail/actions/runs/41173564341) passed the first 26 guard tests, live release check, fresh default-source Release build and **5/5 CTest suites in 28.36 seconds**, using Qt 6.12.0 / OpenSSL 3.6.5. The updated 28-test run is in progress; PR has not yet merged.
- GitHub main protection was enabled and read back: PR required, zero outside approvals, enforce_admins=true, strict required `release-gate` from GitHub Actions app 15368, conversation resolution required, force-push/deletion forbidden. The first protected merge and main push run remain to be verified.
- Existing `v0.1.0-rc.1` remains unchanged; this task does not modify or publish upstream components. The guard complements fixed versions and cannot infer untested changes in another developer workspace.

## UI dependency release pin · 2026-10-31

- Replaced the default sibling workspace with a canonical Git submodule at `third_party/nextkdeAppDemo`, fixed to source prerelease [`v0.1.0-rc.1`](https://github.com/NextKde/nextkdeAppDemo/releases/tag/v0.1.0-rc.1), commit `dd0d5f3e7a7eab75c7bd465f29709d5f86e8091d`. The annotated tag and GitHub prerelease are published; this is not a stable/binary SDK release.
- Fresh default-source Release configure/build succeeded on Qt 6.12.0 / GCC 16.2.1. Existing CTest suites passed **5/5 in 32.82 seconds** (CLI, software/offscreen UI, core, controller, loopback protocol); no real mailbox was accessed. A separate clean recursive clone of integration commit `2a697b1d48fc632ecc8191be25ec7e432c765171` initialized the UI source from GitHub and independently built/passed **5/5 in 29.89 seconds**. Both runs used the exact commit now tagged for the release.
- Verified an uninitialized clean source snapshot fails with the documented submodule-init command, the fresh default cache points inside this checkout's submodule, both consumer copies have the exact clean candidate SHA, and an explicit sibling development override still configures. Existing Qt/QML tooling/plugin discovery and upstream mailio compiler warnings remain; no new qmllint, standalone Demo or GPU acceptance is claimed.
- CMake retains an explicit `NKMAIL_UI_SOURCE_DIR` development override. Default builds neither follow a moving branch nor fall back to the sibling workspace. Existing caches must be reset or replaced to verify the pin.
- Historical 2026-10-29 source/acceptance facts below remain historical; they did not use this release pin. The 2026-10-30 skeleton record is likewise a prior iteration, subsequently committed as `d47833d`, not the current working-tree state.

## Prior UI iteration · 2026-10-30

The user rejected the previous shell presentation. The current uncommitted iteration is **skeleton only**, not visual acceptance or a published milestone:

- Replaced the empty 30px chrome band plus pane-local toolbars with one real 68px header owning sidebar/list/reader actions and search. Sidebar body and list/reader content now start at y=68 in separate shell containers.
- Test loading now uses the compiled TestViews module so MailStyle singleton dimensions resolve. Strict header/body geometry, control ownership, pointer input, compact details anchoring and eight width boundaries are tested; warnings fail UI tests.
- Local build succeeded. Direct software/offscreen UI run: **18 passed, 0 failed, no QML warnings** (including init/cleanup). Full isolated CTest: **5/5 passed, 29.75 seconds**.
- Actual Wayland/GPU synthetic screenshots inspected: wide light/dark and 580px list/reader, in `tmp/skeleton-review/`. No black frame or header/control overlap observed in those four captures. This is not user approval.
- Existing demo PID 120340 was left running; it still shows the earlier layout. Capture processes exited after saving. No second persistent demo was started, no real mailbox contacted, no commit or push performed.
- Next: user inspection of the structural layout before any detail work. Provider compatibility remains unverified. qmllint was not validated in this iteration; runtime loading and tests are the checks reported here.

## Published milestone (prior to the current UI review)
First runnable milestone published. Second workflow/window milestone published as `d5d5b7d46cfa3a6435a283b0b1b3c505d747abbe`, including two further reviewed synthetic screenshots. Final committed code verified in **Debug: 5/5 CTest passed, 27.96 seconds** and **Release: 5/5 passed, 27.92 seconds**. Coverage: core 16 / controller 23 / UI 14 pass (Qt tests include init/cleanup), CLI 3 process cases, protocol 32 loopback scenarios. Working tree was clean after publication; no real mailbox data or credentials were included.

The scoped local implementation is substantially complete, but real-provider read-only verification is blocked at an unidentified authenticated phase. Do not report the whole first release or real-account compatibility complete. Next useful work is credential-free phase diagnostics and controlled resolution with account-owner input, not repeated password attempts or arbitrary UI expansion.

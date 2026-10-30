# NkMail local delivery goal

## Contract
Build a working single-account Qt Quick mail client in this directory. Reuse canonical Kos.Ui from the sibling nextkdeAppDemo project; use mailio for IMAP/SMTP/MIME. Follow KMusic application patterns and calendar's dialog/popover interactions, with macOS Mail as a visual reference. No server-side flag editing, move or delete in the first release.

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

## Current work · 2026-10-30

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

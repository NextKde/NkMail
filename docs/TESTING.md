# Testing and acceptance boundaries

NkMail has five distinct local suites. Enable them with `-DNKMAIL_BUILD_TESTS=ON` and run `ctest --test-dir build --output-on-failure`.

| Suite | Actual code exercised | Deliberately isolated boundary |
| --- | --- | --- |
| `nkmail_core` | SQLite migrations/cache/drafts, DTO serialization, incremental models | Temporary databases; no keyring jobs or network |
| `nkmail_controller` | Real controller, SQLite and message model; asynchronous dispatch and stale-result handling | Linked synthetic backend and credential-store implementations; no network/keyring implementation |
| `loopback-protocol` | Real mailio adapter, patched TLS/IMAP/SMTP, MIME encoding/decoding | Python services bound to loopback, generated test CA/certificates, synthetic credentials and `.invalid` recipients; no relay |
| `nkmail_ui` | Real QML views and canonical Kos.Ui components, focus and editor interactions | Behavioral controller mock; offscreen software rendering |
| `nkmail_cli` | Actual application process, demo capture guard, private data directory and process locking | Fresh XDG directories, no configured account, synthetic demo |

A controller mock or linked boundary replacement is not protocol acceptance. Conversely a passing protocol fixture does not prove that QML/controller workflows use it correctly; this is why the suites are separate.

## Screenshots

```sh
./build/nkmail --demo --appearance=light --screenshot=tmp/inbox-light.png
./build/nkmail --demo --appearance=dark --review-scenario=details --screenshot=tmp/details-dark.png
./build/nkmail --demo --review-scenario=compose --screenshot=tmp/compose.png
./build/nkmail --demo --review-width=680 --review-height=760 --screenshot=tmp/narrow.png
```

Run these in a real graphical session for GPU/material review. The CLI suite's offscreen screenshot checks only isolation and PNG export, not final appearance. Review every image before copying it to `docs/progress/`. Screenshot export requires `--demo`, which always uses a fresh temporary mailbox and cannot send or change accounts.

## Manual acceptance checklist

Use a disposable test data directory, not another mail application's profile.

1. Empty account: configure manual TLS endpoints; password remains empty when reopening the dialog. Saving configuration is not an authentication success indicator. Refresh explicitly starts the connection.
2. Read: refresh folders and headers, paginate, choose an uncached message, save a selected attachment to a new destination. Reading must not change server Seen/flag state. Never use destructive test commands against real mailboxes.
3. Offline: disconnect, restart, read previously cached content, search cached headers/body, paginate cached results. Uncached content must be labeled unavailable rather than replaced by another message.
4. Drafts: create, auto-save, close/reopen, restart, add/remove attachments, reply/reply-all/forward, discard. Review generated recipients before any submission.
5. SMTP: use the loopback fixture for acceptance/rejection/ambiguous-result tests. Do not submit to a real recipient without separate permission. Unknown must remain non-resendable; accepted is not delivered and does not imply a server Sent copy.
6. Window: resize both column separators by pointer and keyboard, narrow/widen, check light/dark, reopen the same profile to restore geometry. Demo/review must not read or update real profile preferences.
7. Credentials: only test live QtKeychain integration with explicit permission. Synthetic boundary tests verify controller behavior, not wallet availability. There is no insecure plaintext fallback.

## Security limits

- Cache, drafts and attachment files are local plaintext sensitive data, protected by filesystem permissions, not encryption at rest.
- Credential keys are scoped to the canonical mailbox data directory. Moving a profile requires re-entering its password. The old preview's global `default` credential is not automatically read or migrated.
- Network timeouts bound individual I/O, not an entire multi-message sync; thread shutdown waits for pending work.
- MIME size/tree checks do not represent a full audit of the upstream parser.
- Read-only provider verification is independent of local acceptance. Never publish real mailbox output, even after a successful private test.

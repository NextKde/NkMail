# NkMail

A native Qt Quick mail client for NextKde. The interface takes cues from macOS Mail, uses canonical **Kos.Ui** from nextkdeAppDemo, and uses **mailio** for IMAP, SMTP and MIME.

> Under active local development. See [GOAL.md](GOAL.md) for the actual acceptance checklist and [progress](docs/progress/README.md) for verified milestones. A rendered preview is not proof of provider compatibility.

## First-release scope

Single-account application-password authentication, folders, paginated headers, on-demand safe text reading, attachments, cached-only search, local durable drafts, compose/reply/reply-all/forward and SMTP submission. No OAuth, remote flag editing, move/delete, IDLE or background daemon. Server acceptance is not delivery; uncertain sends are not retried automatically.

Mailio is a protocol/MIME library, not a complete application backend. NkMail supplies the application state, cache, credentials, UI and workflows. The pinned dependency requires a reviewed TLS verification and PEEK patch before any real server acceptance.

## Dependencies

- CMake 3.30+, C++20 compiler, Ninja (optional)
- Qt 6.8+: Core, Gui, Qml, Quick, QuickControls2, QuickDialogs2, Network, Concurrent, Sql; QtTest/QuickTest for testing
- Qt graphical-effects compatibility module and SVG support as required by Kos.Ui
- QtKeychain for Qt 6; system Secret Service or equivalent
- Boost development files (`date_time`, `regex`) and OpenSSL
- Sibling `../nextkdeAppDemo/shared/qml` (or set `NKMAIL_UI_SOURCE_DIR`)

Mailio 0.26.1 source commit: `f358b1f85f9a021f0bd814e285eb8ae102fc8f79`.
Kos.Ui workspace base: `54fd5f14a5bc50132d524d053b373b1dc1189935` with pre-existing local component updates. NkMail consumes this canonical workspace as-is; that SHA alone does **not** reproduce the current components. No component sources are copied or changed here.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
./build/nkmail --demo
```

Use `./build/nkmail --data-dir /a/private/directory` for a separate persistent mailbox. Preview mode always uses a fresh temporary directory, never existing credentials, and never the network.

```sh
./build/nkmail --demo --appearance=light --screenshot=tmp/inbox-light.png
./build/nkmail --demo --appearance=dark --review-scenario=compose --screenshot=tmp/compose-dark.png
```

Screenshot export is intentionally blocked outside synthetic preview mode. Public progress images must never expose a personal account, real mail or credentials. Test SMTP services bind only loopback and do not relay.

## Privacy and safety

Passwords belong in the system keychain, never in settings, SQLite, source control or logs. A locked/unavailable keychain falls back to the current session only, not a plaintext password file. Cached messages are local sensitive data and are **not encrypted at rest**; protect the data directory accordingly. Remote HTML resources are not loaded. Attachments are never automatically executed or opened.

Do not use a primary personal mailbox until the documented protocol and TLS acceptance checks are complete. Real-provider verification and external delivery are separate from local loopback tests.

## Design references

- [Apple Mail user guide](https://support.apple.com/guide/mail/welcome/mac): restrained message hierarchy, compact toolbar groups and a spacious reading pane. Reference artwork is not redistributed.
- Local KMusic: application bootstrap, right-hand window controls and controller/model patterns.
- Local calendar: anchored content popovers, modal confirmations and fixed editor footers.

## License

GPL-3.0-or-later for NkMail. See LICENSE and NOTICE.md. Third-party components retain their own licenses.

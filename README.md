# NkMail

A native Qt Quick mail client for NextKde. The interface takes cues from macOS Mail, uses canonical **Kos.Ui** from nextkdeAppDemo, and uses **mailio** for IMAP, SMTP and MIME.

> Under active local development. See [GOAL.md](GOAL.md) for the actual acceptance checklist and [progress](docs/progress/README.md) for verified milestones. A rendered preview is not proof of provider compatibility.

## First-release scope

Single-account application-password authentication, folders, paginated headers, on-demand safe text reading, attachments, cached-only search, local durable drafts, compose/reply/reply-all/forward and SMTP submission. No OAuth, remote flag editing, move/delete, IDLE or background daemon. Server acceptance is not delivery; uncertain sends are not retried automatically.

Mailio is a protocol/MIME library, not a complete application backend. NkMail supplies the application state, cache, credentials, UI and workflows. The vendored dependency includes the documented [TLS verification and PEEK patch](patches/mailio/0001-verified-tls-peek-timeouts.patch); its loopback acceptance tests do not by themselves prove real-provider compatibility.

## Dependencies

- CMake 3.30+, C++20 compiler, Ninja (optional)
- Qt 6.8+: Core, Gui, Qml, Quick, QuickControls2, QuickDialogs2, Network, Concurrent, Sql; QtTest/QuickTest for testing
- Qt LinguistTools (`lupdate`/`lrelease`) and Chinese Qt translation catalogs (`qtbase`, `qtdeclarative`); Arch packages: `qt6-tools`, `qt6-translations`
- Qt graphical-effects compatibility module and SVG support as required by Kos.Ui
- QtKeychain for Qt 6; system Secret Service or equivalent
- Boost development files (`date_time`, `regex`) and OpenSSL
- Pinned Kos.Ui source submodule at `third_party/nextkdeAppDemo/shared/qml`

Mailio 0.26.1 source commit: `f358b1f85f9a021f0bd814e285eb8ae102fc8f79`.
Kos.Ui source prerelease: [`v0.1.0-rc.1`](https://github.com/NextKde/nextkdeAppDemo/releases/tag/v0.1.0-rc.1),
commit `dd0d5f3e7a7eab75c7bd465f29709d5f86e8091d`. The Git submodule records that exact
canonical source revision, not a moving branch or an application-specific component copy.
System dependencies above must still be installed.

## Clone and build

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

### Required UI release gate

Before opening an integration PR, stage the intended gitlink and lock-file updates,
then run:

```sh
python3 -m unittest discover -s tests/dependency -v
python3 scripts/check_ui_dependency.py
```

`third_party/kos-ui.lock.json` records the chosen tag and full SHA. The guard compares
it with the staged gitlink and clean submodule HEAD, requires the canonical URL and
no tracking branch, and checks the upstream published GitHub Release and actual tag
commit (including annotated tags). Prereleases are allowed; missing releases, dirty
submodules, mismatched commits and API/network errors fail the check. The script is
read-only and never submits or publishes shared changes for you.

For a new shared component, the order is: develop/test upstream → commit and push it
upstream → publish its source release → check out that release in NkMail's submodule
→ update/stage the gitlink, lock and source notices → run the guard → open a PR.
Running `git add` in NkMail does **not** commit files inside its submodule. For example,
after upstream publishes `v0.1.0-rc.2` (do not run before it exists):

```sh
git -C third_party/nextkdeAppDemo fetch origin tag v0.1.0-rc.2
git -C third_party/nextkdeAppDemo checkout --detach v0.1.0-rc.2
git -C third_party/nextkdeAppDemo rev-parse HEAD
# Put that tag and full SHA in third_party/kos-ui.lock.json; update README/NOTICE.
git add third_party/nextkdeAppDemo third_party/kos-ui.lock.json README.md NOTICE.md
python3 scripts/check_ui_dependency.py
```

The checker deliberately accepts a staged dependency upgrade before the parent commit,
so you can fix a missing release before opening the PR. It does not need a locally
installed Git hook, and does not silently repair the dependency.

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

## Local regression tests

Tests additionally require Qt Test / QuickTest, Python 3 and the `openssl` executable (for short-lived loopback certificates). They do not need a real account. Core/controller tests use temporary data directories; protocol services bind only `127.0.0.1`; UI tests run offscreen with synthetic state.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNKMAIL_BUILD_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

For visual review, run the actual app rather than treating offscreen tests as a GPU/material check. See the [testing guide](docs/TESTING.md) for suite boundaries and manual acceptance, and the [progress reports](docs/progress/README.md) for exact verified results and known gaps.

## Privacy and safety

Passwords belong in the system keychain, never in settings, SQLite, source control or logs. A locked/unavailable keychain falls back to the current session only, not a plaintext password file. Cached messages are local sensitive data and are **not encrypted at rest**; protect the data directory accordingly. Remote HTML resources are not loaded. Attachments are never automatically executed or opened.

Do not use a primary personal mailbox until the documented protocol and TLS acceptance checks are complete. Real-provider verification and external delivery are separate from local loopback tests.

## Design references

- [Apple Mail user guide](https://support.apple.com/guide/mail/welcome/mac): restrained message hierarchy, compact toolbar groups and a spacious reading pane. Reference artwork is not redistributed.
- Local KMusic: application bootstrap, right-hand window controls and controller/model patterns.
- Local calendar: anchored content popovers, modal confirmations and fixed editor footers.

## License

GPL-3.0-or-later for NkMail. See LICENSE and NOTICE.md. Third-party components retain their own licenses.

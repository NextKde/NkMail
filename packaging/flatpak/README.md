# NkMail Flatpak

NkMail uses `org.kde.Platform/x86_64/6.11`, the same shared Qt runtime as
[NextKde/kos-apps](https://github.com/NextKde/kos-apps). The matching
`org.kde.Sdk` and `io.qt.qtwebengine.BaseApp//6.11` are build dependencies.
WebEngine is absent from the shared platform; the BaseApp supplies it under
`/app`. Basic Qt libraries are not copied from the host. App translations stay
in the bundle. Currently only x86_64 is built and advertised.

The app ID is `org.nextkde.NkMail`, branch `stable`. Native installations retain
their existing identity; their profiles are not automatically migrated into
Flatpak. Both installations respect the same session-wide single-instance bus.

## Build in a disposable VM or CI

Install flatpak-builder, OSTree, the matching SDK/runtime/BaseApp and Python 3.
Initialize the pinned submodule with `git submodule update --init --recursive`.
From a clean committed checkout, run:

```sh
python3 packaging/flatpak/build_bundle.py
```

The producer replaces the local development source with the exact source SHA,
verifies the canonical UI pin, records runtime/SDK/BaseApp commits, builds a
stable ref and exports bundles without online origin/runtime-repository URLs.
`flatpak-release.json` follows appCatalog schema 2, using exact metadata bytes
and installed size from libflatpak. A `.Sources` bundle and full NkMail source
archive accompany the application. The release includes the exact build
manifest and dependency commits. BaseApp sources and notices are maintained
at https://github.com/flathub/io.qt.qtwebengine.BaseApp (branch/6.11).

The `Flatpak package` workflow builds in a GitHub VM and checks exact-commit
installation, offline synthetic inbox/composer/account rendering, reinstall,
and uninstall retaining private data. It does not use real mail or credentials.
Inspect the synthetic screenshots before release. This first release has no
older Flatpak version to test a cross-version upgrade against. Portal file
selection, a desktop keyring and tray interaction require desktop acceptance.

## Permissions and behavior

- Network: IMAP, POP3, SMTP and explicitly permitted remote images. TLS checks
  remain enabled. Publishing a package does not authorize any live mail tests.
- Wayland/fallback X11, IPC and DRI: Qt Quick and WebEngine rendering.
- Notifications and StatusNotifierWatcher: message alerts and tray integration.
- `org.NextKde.NkMail`: existing shared single-instance ownership/activation.
- Secret Service and KWallet: QtKeychain secure storage, with insecure fallback
  disabled. An unavailable/unlocked keyring produces an error, never plaintext.
- File dialogs use the desktop portal; no blanket home or host filesystem access.
- WebEngine retains its Flatpak subprocess sandbox. No `--no-sandbox` override.

The release uses application-password authentication. Google browser OAuth is
not configured in this package; no private project configuration or fixture
OAuth client is embedded. The existing UI explains the available sign-in mode.

## Publication

Main changes require a PR and passing `release-gate`; also require the Flatpak
package workflow to pass before publishing this package. Publish the exact
reviewed artifacts as a stable `vX.Y.Z` Release matching CMake/AppStream version.
Do not overwrite a published version. Add `apps/org.nextkde.NkMail.json` to
[appCatalog](https://github.com/NextKde/appCatalog) with manifest
`flatpak-release.json`. This application uses the catalog's supported unsigned
publisher mode (GitHub ownership + HTTPS + SHA-256); the catalog itself must
still be signed by its maintainer. Package publication and signed catalog
publication are separate operations.

# Install NkMail

NkMail is an early Linux mail client. The Flatpak package targets **x86_64 Linux**
and uses the shared KDE 6.11 runtime. The older native preview package targets
Arch Linux only. Windows and macOS packages are not provided.

## Flatpak

Download `org.nextkde.NkMail-0.1.0-x86_64.flatpak` and `flatpak-release.json`
from the [stable release](https://github.com/NextKde/NkMail/releases/tag/v0.1.0).
Use the bundle SHA-256 recorded in that manifest to check your download. Install
the shared runtime from an approved Flathub source, then install the local bundle:

```sh
flatpak --user remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak --user install flathub org.kde.Platform//6.11
flatpak --user install --bundle ./org.nextkde.NkMail-0.1.0-x86_64.flatpak
flatpak run org.nextkde.NkMail
```

If your installation already has this runtime from kos-apps, Flatpak reuses it.
The bundle includes WebEngine; it does not use your distribution's Qt libraries.
A desktop portal and an unlocked Secret Service/KWallet are needed for file
selection and remembered passwords. Files are selected through the portal;
NkMail has no general access to your home directory. Private app data is under
`~/.var/app/org.nextkde.NkMail/`, separate from the native application's profile.
There is no automatic profile migration. Quit a native NkMail instance before
starting the Flatpak, because both respect the same session-wide instance owner.

Install a later release bundle with `flatpak --user install --bundle <bundle>`.
Uninstall with `flatpak --user uninstall org.nextkde.NkMail`; omit `--delete-data`
to retain your local mail and drafts. The appCatalog client can manage releases
after the maintainer publishes a signed catalog containing this application.
See [packaging and validation details](../packaging/flatpak/README.md).

## Arch Linux package

Download `nkmail-0.1.0rc1-1-x86_64.pkg.tar.zst` and `SHA256SUMS` from the
[release page](https://github.com/NextKde/NkMail/releases). Verify the downloaded
file before installing (the checksum manifest also lists optional source files):

```sh
sha256sum --ignore-missing -c SHA256SUMS
sudo pacman -U ./nkmail-0.1.0rc1-1-x86_64.pkg.tar.zst
nkmail
```

Use an up-to-date Arch system; pacman installs declared dependencies. Start NkMail
from the application launcher or run `nkmail` in your graphical desktop session.
The desktop entry and blue envelope icon are installed with the package. A session
D-Bus is required. For remembered passwords, unlock your desktop keychain/Secret
Service (for example GNOME Keyring or KWallet). There is no plaintext credential
fallback. `nkmail --demo` opens synthetic preview data only when no interactive
instance is already running. Quit the existing instance before switching modes.

Uninstall with `sudo pacman -R nkmail`. This leaves your account data and keychain
entries intact. The preview has no automatic updater or login autostart.

## Build from source on Linux

Install CMake 3.30+, a C++20 compiler, Qt 6.8+ development packages (including
WebEngine Quick, Quick Dialogs, DBus, Widgets and LinguistTools), QtKeychain for
Qt 6, Boost date_time/regex and OpenSSL. Qt SVG, Qt 5 compatibility graphical
effects and Qt translations must be available at runtime. See README for the
complete module list. Older distribution Qt packages may not meet these versions.

```sh
git clone --recurse-submodules https://github.com/NextKde/NkMail.git
cd NkMail
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/release --parallel 2
cmake --install build/release
"$HOME/.local/bin/nkmail"
```

Alternatively extract our `nkmail-0.1.0rc1.tar.gz` release asset, which includes
the pinned Kos.Ui sources, and run the CMake commands inside it. GitHub's automatic
“Source code” archives omit submodules and are **not** the complete source bundle.
The binary package is built from the complete bundle; `SOURCE_REVISION` identifies
both commits. `BUILD-DEPENDENCIES.txt` records its build environment. The supplied
PKGBUILD and source tarball can also be rebuilt together with `makepkg` on Arch.

Ordinary desktops do not need NextKde's compositor extensions: application-drawn
window controls remain available. Native builds require system Qt/WebEngine libraries;
copying the `nkmail` executable alone to a different distribution is unsupported.

## First use and current limitations

Add an IMAP or POP account with SMTP and your provider's application password
where required. Saving a verified account starts the first inbox fetch; errors
provide a retry. Microsoft OAuth/Exchange sign-in is not implemented. Provider
presets are configuration helpers, not a claim that every provider has been
certified. POP deletion is local; server copies are preserved.

This preview is not a full mailbox mirror: older messages load on demand, and
smart/unified mailbox navigation is deliberately hidden. Cache files are not
encrypted at rest. Package tests use synthetic data and do not prove real-provider
compatibility or external delivery. The package bundles no developer OAuth client,
credentials, personal mail or private machine configuration.

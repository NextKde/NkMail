# Install NkMail

NkMail is an early preview for Linux. The first binary distribution targets
**Arch Linux x86_64**, with dependencies supplied by Arch repositories. It is not
an AppImage and is not a package for Ubuntu, Debian, Fedora, Windows or macOS.

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
window controls remain available. System Qt/WebEngine libraries are required;
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

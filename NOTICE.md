# Third-party notices

## NkMail
Application source is licensed GPL-3.0-or-later; see LICENSE. No warranty is provided.

## Kos.Ui / nextkdeAppDemo
NkMail builds canonical Kos.Ui from the Git submodule at third_party/nextkdeAppDemo, sourced from https://github.com/NextKde/nextkdeAppDemo.git. The current commit-only pin is 2ba0039b0be6e9326117e51cc94aac0ad99bc69a, as recorded in the gitlink and kos-ui.lock.json; no release tag is claimed for that pin. This is an upstream gitlink, not a separately maintained component copy or fork. This component library is GPLv3 licensed; its own LICENSE and NOTICE.md apply and remain in the submodule. An explicit NKMAIL_UI_SOURCE_DIR override is available for local development only. The local KMusic and calendar applications are architectural/interaction references; their repositories are not copied wholesale.

## mailio
Vendored source: https://github.com/karastojko/mailio
Version 0.26.1, commit f358b1f85f9a021f0bd814e285eb8ae102fc8f79.
Simplified BSD license, copyright (c) 2016 Tomislav Karastojković.
The complete license is retained at third_party/mailio/LICENSE. NkMail-specific security/PEEK, LIST special-use attribute preservation RFC 2971 client ID and SASL XOAUTH2 modifications are documented in patches/mailio/ and do not imply upstream endorsement.

## Other linked dependencies
Qt (LGPL/GPL/commercial as applicable to the modules and distribution), QtKeychain (modified BSD), Boost (Boost Software License), OpenSSL (Apache-2.0). Distribution must preserve their applicable notices and fulfill their licenses. These system libraries are not copied here.

## Visual references
Apple Mail user-guide screenshots were consulted for design research only. Apple artwork, branding and screenshots are not redistributed as NkMail assets. Progress screenshots depict NkMail with synthetic data.

## Provider marks

Account setup bundles provider identity images for offline display. Brand rights remain with their owners; no endorsement is implied. Sources are recorded in data/providers/README.md.

## Flatpak packaging

`packaging/flatpak/bundle_metadata.py` adapts the exact-byte libflatpak reader
from NextKde/kos-apps (`e5bab138f8cbfab697b5d67c1f4a1cc653fb29b4`), GPL-3.0.
Flatpak builds bundle Boost 1.88.0 (Boost Software License 1.0), QtKeychain
0.15.0 (modified BSD license), and the QtWebEngine BaseApp 6.11
(Qt/Chromium and bundled third-party notices). Dependency license files are
retained in the package and corresponding source bundles accompany releases.
KDE Platform 6.11 is a separately installed shared runtime.

The Flatpak also builds the canonical `Kos.SurfaceShape` Wayland protocol module
from NextKde/NextKde at `d7b34a8e8b37a9fb7605333f10d02f7b16e70743`
(`shell/native/surface-shape`), GPL-3.0. Its license and corresponding source
are included in the package and Sources bundle. No host plugin binary is copied.

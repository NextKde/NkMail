# Third-party notices

## NkMail
Application source is licensed GPL-3.0-or-later; see LICENSE. No warranty is provided.

## Kos.Ui / nextkdeAppDemo
NkMail builds canonical Kos.Ui from the Git submodule at third_party/nextkdeAppDemo, sourced from https://github.com/NextKde/nextkdeAppDemo.git. The pinned source prerelease is v0.1.0-rc.1, commit dd0d5f3e7a7eab75c7bd465f29709d5f86e8091d. This is an upstream gitlink, not a separately maintained component copy or fork. This component library is GPLv3 licensed; its own LICENSE and NOTICE.md apply and remain in the submodule. An explicit NKMAIL_UI_SOURCE_DIR override is available for local development only. The local KMusic and calendar applications are architectural/interaction references; their repositories are not copied wholesale.

## mailio
Vendored source: https://github.com/karastojko/mailio
Version 0.26.1, commit f358b1f85f9a021f0bd814e285eb8ae102fc8f79.
Simplified BSD license, copyright (c) 2016 Tomislav Karastojković.
The complete license is retained at third_party/mailio/LICENSE. NkMail-specific security/PEEK modifications are documented in patches/mailio/ and do not imply upstream endorsement.

## Other linked dependencies
Qt (LGPL/GPL/commercial as applicable to the modules and distribution), QtKeychain (modified BSD), Boost (Boost Software License), OpenSSL (Apache-2.0). Distribution must preserve their applicable notices and fulfill their licenses. These system libraries are not copied here.

## Visual references
Apple Mail user-guide screenshots were consulted for design research only. Apple artwork, branding and screenshots are not redistributed as NkMail assets. Progress screenshots depict NkMail with synthetic data.

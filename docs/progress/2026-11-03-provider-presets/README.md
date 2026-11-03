# Provider presets · 2026-11-03

The account wizard now offers iCloud, Gmail, QQ, Tencent Exmail, 163, 126, Yahoo, AOL and manual IMAP/SMTP. Selecting a provider fills TLS server settings and explains application-password requirements. Unknown custom domains are not guessed. Microsoft/Exchange is explicitly unavailable until its authentication path is implemented.

ProviderPicker and ProviderProfiles own presentation and presets. AccountSetup and the existing mail services still own verification, receiving and sending. Protocol changes add advertised IMAP ID negotiation, special-use Sent/All recognition and an account option to avoid duplicate sent copies when the server saves them (default for the Gmail preset).

Provider rows now display bundled official-site brand images, preserving their colors and aspect ratios; Other retains the generic envelope. No logo is fetched at runtime. Sources are recorded in `data/providers/README.md`.

![Synthetic Chinese account picker](providers.png)

This is a synthetic host Wayland capture; no personal mail or credentials are included. Shared Kos.Ui source/materials are unchanged. Compositor-owned window buttons are outside application captures.

Validation: full CTest suite, including UI, translations, controller and 44 synthetic loopback protocol/TLS scenarios; dependency gate and 32 dependency regressions. Results are recorded in GOAL.md after execution. No live provider account was tested and no real message was sent. OAuth, token refresh and native Exchange remain unimplemented. See [provider scope and sources](../../PROVIDERS.md).

Logo follow-up validation: host build, UI suite (11.14s), catalog/i18n checks and canonical dependency gate passed. Every branded resource is decoded in the UI regression.

![Synthetic English dark provider picker](providers-dark.png)

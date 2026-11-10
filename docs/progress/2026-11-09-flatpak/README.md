# Flatpak integration · 2026-11-09

Application ID: `org.nextkde.NkMail`, x86_64, stable branch. Shared runtime:
`org.kde.Platform//6.11`; WebEngine comes from the matching BaseApp. The pinned
Kos.Ui source is unchanged. Packaging and permissions are documented in
[the packaging guide](../../../packaging/flatpak/README.md).

These three synthetic screenshots were exported by the installed candidate at
source `85267463141c5fcc8d10c2d3b7a7a67144784450` in a GitHub-hosted VM, with network
access removed. They show the inbox/WebEngine reader, composer and account dialog.
All addresses use `.invalid`; no real account or mailbox was accessed. Images were
visually inspected. Xvfb rendering does not establish compositor-specific visual
acceptance or desktop keyring/file-dialog interaction.

That candidate is **not accepted for publication**: normal startup exposed a
D-Bus permission ordering error (`talk` replaced `own` on the single-instance
name). The follow-up removes the duplicate and verifies actual exported metadata
before producing a release plan. The existing release-gate and Arch package passed.
Final lifecycle acceptance and the stable Release are tracked in
[PR #33](https://github.com/NextKde/NkMail/pull/33). The user will handle the signed
appCatalog publication after the application Release and catalog submission PR.

![Synthetic inbox](inbox.png)
![Synthetic composer](compose.png)
![Synthetic account dialog](account.png)

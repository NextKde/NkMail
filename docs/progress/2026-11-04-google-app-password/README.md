# Gmail app-password setup · 2026-11-04

Google browser OAuth is no longer exposed in account setup. Google / Gmail uses
email plus app password, with Gmail IMAP/SMTP presets and explicit two-step
verification guidance. A configured OAuth client does not restore the browser
sign-in button. Google Cloud setup is not needed for this workflow.

The retained OAuth backend supports existing accounts. Editing one allows a
verified switch to password authentication using the same account ID and cached
mail. Missing/expired credential prompts direct users to account settings. No
account or credential is automatically converted, removed or revoked.

`setup.png` is an actual VM screenshot using synthetic `.invalid` addresses and
demo mode. Fields and saving are disabled in that capture; it demonstrates layout
and guidance, not real provider authentication. No private mail or credentials
are included.

VM Release build passed. UI/controller/i18n/catalog checks passed 4/4 in 53.80s.
The controller fixture verifies switching the authentication method preserves the
account identity and cached body; UI fixtures verify the app-password path even
with OAuth configured. Final backend prompt build and controller/i18n/catalog follow-up passed 3/3 in 6.23s.
No host launch/tests or real mailbox writes were performed for this change.

# Project-managed Google sign-in

Users select Google and click **Sign in with Google**. NkMail opens the system browser,
checks the returned verified email, and saves the account. No Google Cloud configuration
is exposed in account settings. An optional app-password path remains for accounts where
Google permits it. Google OAuth accounts use secure Gmail IMAP/SMTP, including custom
Workspace email domains; POP is not implemented.

## One-time distributor configuration

1. Create a Google Cloud project owned by the NkMail publisher. Configure Google Auth
   Platform branding, support/developer contact, audience and the required policy agreement.
2. Create an OAuth client of type **Desktop app**, owned by this project. Do not reuse
   Apple, Thunderbird or another application's client identity.
3. Declare `openid`, `https://www.googleapis.com/auth/userinfo.email` and
   `https://mail.google.com/`. The last is Google's restricted scope for IMAP/SMTP,
   whose consent text includes reading, sending and deleting mail. Declaring the scope
   does not itself grant access to a user's mailbox; each user authorizes in Google.
4. During development, retain Testing and list the intended test accounts. Testing mode
   is limited and refresh credentials can expire after seven days. A successful fixture
   test or creation of a desktop client does not establish public-release readiness.
5. Before public distribution, complete branding with publisher-owned homepage/privacy
   information, verified domains and Google's applicable OAuth/restricted-scope review.
   The publisher must assess the requirements for its actual architecture and distribution;
   do not publish placeholder privacy claims or bypass consent warnings.
6. Save the downloaded installed-client JSON outside the repository. Keep its backup
   and release-system access under project ownership. Never commit real credentials.

Build the official binary with:

```sh
cmake -S . -B build/release \
  -DNKMAIL_GOOGLE_OAUTH_CLIENT_FILE=/private/path/google-oauth-client.json \
  -DNKMAIL_REQUIRE_GOOGLE_OAUTH=ON
cmake --build build/release
```

The JSON is bundled into the executable resource `:/oauth/google-client.json`; it is
not read from a user's profile or environment. Desktop clients cannot keep an embedded
client secret confidential: this is a public installed-client identity, not a substitute
for PKCE or consent. Build configuration rejects non-installed JSON. A build with no
configuration disables Google sign-in and explains that the distributor must provide it.
CI uses synthetic project JSON and never needs a real Google credential. Official
release automation must supply its project JSON securely; ordinary source CI artifacts
are not automatically configured official Google releases.

## Consent troubleshooting

Google can grant basic sign-in identity without granting Gmail mail access. On the
Google consent page, allow Gmail mail access to connect the mailbox. The layout can
vary depending on existing grants; a checkbox is not always shown. NkMail
checks the granted scope and refuses to save a mailbox without it. A separate message
identifies a missing offline refresh token; do not confuse this with missing mail scope
or a TCP connection failure. Retrying opens explicit consent again. NkMail never
silently substitutes password authentication or accepts a partial grant as success.

## Token and protocol behavior

Authorization uses PKCE S256, random state, a short-lived IPv4 loopback listener and
strict callback validation. Google's endpoints are fixed in code; JSON endpoint overrides
are ignored. Tokens are exchanged with TLS verification, response limits, timeouts,
manual redirects and no browser cookies. NkMail does not receive the Google password.

The verified identity is taken from the ID token returned directly by Google's fixed
token endpoint during this authorization-code exchange. This private path authenticates
the response through verified TLS, as permitted by OIDC Core 3.1.3.7 step 6 and Google's
server-flow guidance. It checks issuer, audience, authorized presenter, one-use nonce,
subject, expiry/issuance times, verified email and any supplied access-token hash. It
rejects `alg=none`, malformed token structure and externally supplied key references.
This is not a general JWT signature-verification API: browser/profile/QML-supplied ID
tokens are never accepted. ID tokens are neither stored nor exposed. A second UserInfo
request is unnecessary, avoiding an extra network dependency after successful exchange.

Only an opaque refresh credential goes through QtKeychain; account SQLite metadata and
QML never contain tokens. Access tokens stay in memory, refresh is single-flight and
refresh-token rotation updates the keychain through AccountSession. Removal/cancellation
invalidates cached credentials; rejected grants require signing in again. Authentication
errors never fall back to a Google password or IMAP LOGIN.

Mailio uses SASL XOAUTH2 over verified TLS for Gmail IMAP and SMTP, including STARTTLS.
Google authorization completes account creation independently of reachability of the
mail ports. Later sync reports connection or authorization failures. SMTP authorization
checks never send mail. Real mailbox writes are outside the synthetic acceptance tests.

## Evidence and remaining acceptance

The OAuth tests use local synthetic authorization/token endpoints and backchannel identity claims and include
PKCE, verified identity, callback rejection, cancellation, refresh concurrency, rotation,
expiry and redirect rejection. Protocol fixtures exercise XOAUTH2 success/failure and
challenge acknowledgment without password fallback. UI tests cover project-configured
one-click login and unavailable-build guidance. Real browser consent and mailbox access
must be reported separately from those fixtures.

Sources: [Google native applications](https://developers.google.com/identity/protocols/oauth2/native-app),
[Gmail XOAUTH2](https://developers.google.com/workspace/gmail/imap/xoauth2-protocol),
[OAuth production readiness](https://developers.google.com/identity/protocols/oauth2/production-readiness/policy-compliance),
[testing restrictions](https://support.google.com/cloud/answer/15549945),
[OIDC code-flow identity validation](https://openid.net/specs/openid-connect-core-1_0.html#IDTokenValidation),
[Google direct token response](https://developers.google.com/identity/openid-connect/openid-connect#obtainuserinfo).

# Mail reminders and unread tray count

IMAP and POP inbox receipt now feeds one durable account-scoped reminder policy,
independent of the selected folder. Initial observations and identity resets stay
quiet. POP tracks the complete UIDL listing to distinguish previously unseen
headers from genuinely new identities. Disabled notification periods advance the
cursor without a later flood. The preference persists inside the selected profile.

An asynchronous freedesktop notification adapter routes default actions by the
returned notification ID. It respects text/markup capabilities and invalidates
IDs across daemon restarts. Controller navigation checks the complete source
identity before switching, retains queued actions through writes/sends, and opens
headers outside the initial visible page. Credential restoration resumes uncached
bodies and deferred read-state updates.

Tray counts reflect all locally synced inboxes. VM-rendered badge samples:

![12 unread](tray-12.png) ![More than 99 unread](tray-99plus.png)

Full VM regression passed 11/11 in 134.11s. Final focused notification/controller/tray/catalog checks passed 4/4 in 11.68s,
including daemon restart and markup/plain-text capability handling. Narrow reader
navigation passed 3/3 including lifecycle; canonical UI dependency gate passed. No real-mail writes or host
runtime validation. A separate Shell fix restricts synthetic tray notifications
to explicit NeedsAttention transitions, so count/icon updates cannot duplicate
native mail notifications or bypass the mail notification preference.

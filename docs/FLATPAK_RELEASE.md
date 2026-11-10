# Publish an accepted Flatpak

After the source PR is merged, dispatch **Publish accepted Flatpak** on `main`
with its successful **Flatpak package** workflow run ID. The original artifact
source commit must be an ancestor of main and have a successful release-gate
workflow. With a squash merge, build a new candidate on main first.

The publisher downloads the original CI bytes; it does not compile or execute
the application. It verifies the bundle identity, exact metadata, installed
size, digests, runtime, D-Bus ownership and absence of online origin URLs.
It uploads a draft, checks GitHub's asset hashes and then publishes `vX.Y.Z`.
Only x86_64 and the current shared KDE 6.11 runtime are supported by this producer.

A matching draft can be resumed only if every existing asset is already uploaded
and identical. A mismatched/incomplete draft stops publication for review.
A published release is never overwritten. The workflow serializes publication
and has repository-content write permission only in the publishing job.

No signing secret is needed for appCatalog's unsigned-publisher mode. Signing
and publishing the appCatalog directory remains a separate maintainer action.
This workflow keeps large transfers on GitHub's build infrastructure.

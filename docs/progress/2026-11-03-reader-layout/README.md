# Reader layout correction · 2026-11-03

The previous table fallback invented pipe characters between cells and emitted breaks for every layout row. Blocked images still reached Qt as img elements without a source, producing broken-image icons. The reader now suppresses blocked image elements while retaining available alt text, compacts empty blocks/row spacers and source whitespace, and removes standalone legacy separator nodes from cached markup. Inline pipes and preformatted text remain intact. New table conversions use flowing blocks and spaces instead of pipe characters. Stored message data is unchanged by presentation cleanup. The reader header also uses translated mailbox labels.

This remains a simplified Qt rich-text view, not full browser/CSS rendering. Remote images remain opt-in. The legacy separator heuristic removes whitespace-only pipe nodes outside preformatted blocks; ordinary inline pipes remain visible.

VM default pinned-source build passed. Final UI/core checks passed 2/2 in 39.05s (49 UI and 22 core QtTest cases including init/cleanup). i18n, translation catalogs and the loopback protocol suite passed in the preceding run; that run's UI comparison was corrected to inspect rendered plain text rather than Qt's reserialized HTML. Targeted capture run passed (3 cases including init/cleanup). Existing compiler missing-initializer warnings remain.

`notification.png` is a synthetic fixture captured in the VM with the offscreen software backend. It verifies readable layout and absence of broken image objects, not GPU liquid rendering. No account identifiers, real mail content or credentials are included. The user's screenshot is not published. The host application was rebuilt and relaunched for manual inspection; automated tests ran only in the VM.

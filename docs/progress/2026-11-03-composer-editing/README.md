# Composer editing tools · 2026-11-03

The reference guides composer content and editing actions. NextKde retains ownership of window corners and decoration, through the existing cooperative WindowChrome/MailWindowControls adapter. Floating header controls retain the canonical same-window liquid backdrop; the formatting strip and popover interiors use ordinary controls.

The composer now exposes body/title/heading text presets, installed font families, regular/bold/italic/bold-italic faces, size and color, selection-aware emphasis, three paragraph alignments, bullet/numbered lists and bounded block indentation. Cursor movement updates format controls. The strip scrolls horizontally in narrow windows. Ctrl+B/I/U and Ctrl+K act only when editing the body.

Header actions include undo/redo, formatting visibility, an anchored emoji picker and an anchored link editor. Links apply to selected text and accept HTTP(S) or mailto addresses. Emoji insertion replaces the current selection in one undo step. Existing attachment, Bcc, send and draft lifecycle behavior remains in place. Apple-only Siri and Hide My Email services are outside this implementation.

Validation is performed only in the development VM, in an isolated source directory. No real account or mailbox operations are used. Screenshot exports show synthetic client content; compositor-owned decorations and corners are outside the exported image.

Validation results: VM build succeeded; UI/i18n/catalog CTest checks passed 3/3 in 30.21s, including 46 passing UI cases. Tests cover the emoji chooser opening/insertion/closing, selected-text links, invalid URL scheme rejection, emoji replacement as one undo step, numbered/bullet conversion, alignment, indentation persistence and undo. Initial alignment-button lookup failure was resolved by using explicit button instances.

Chinese light/dark client captures were inspected using the VM's offscreen software backend. All format actions remain visible at the initial size, with a scrollbar for narrow windows. Wayland capture reported that the preview was not exposed, so these images establish content layout and opaque fallback only; they do not validate liquid refraction or compositor decoration. No host compilation, tests or GUI execution were performed.

- `compose-light.png`: Chinese synthetic light composer, software fallback.
- `compose-dark.png`: Chinese synthetic dark composer, software fallback.

## Compact formatting groups

The user's follow-up specifies small, non-rounded button groups for the formatting strip. Emphasis, alignment, lists and indentation now each share a rectangular ordinary surface with 24px-high square action cells; the font selectors and color control match the compact height. The app composes existing shared action controls without changing the pinned UI library. Header floating controls retain their existing material and shape.

VM build and existing UI/i18n/catalog checks passed 3/3 in 30.38s (46 UI cases). The final icon-only/padding adjustment was rebuilt and reviewed in refreshed light/dark software-fallback exports. No host build, test or GUI execution was performed.

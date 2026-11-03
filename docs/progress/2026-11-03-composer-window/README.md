# Independent mail composer · 2026-11-03

The composer now opens as a separate nonmodal window with close/minimize/maximize controls and resize handles. Its layout follows the supplied macOS Mail reference: a compact top action strip, full-width address/subject rows, a horizontal format strip, and an expanding unboxed editor. The existing canonical Kos.Ui controls remain pinned and unchanged.

Closing the composer offers Save / Don’t Save / Cancel. Save failure keeps the content available. Don’t Save deletes the local draft and stops autosave. Active or pending sends block close/discard; accepted sends close only the matching draft. Switching accounts disables editing until the original account is active again. Local drafts also expose a confirmed trash action in the list.

Formatting supports font family/size, color, bold, italic, underline, strikethrough and bullet lists. TextSelection handles selected text and insertion formatting; list operations receive actual editor selection coordinates. Plain draft newlines are preserved when loading the rich editor.

Validation: the user explicitly authorized host testing for this iteration. Host build passed; UI (42 cases), i18n and catalog checks passed, 3/3 in 15.76s. Chinese light/dark composer and close-decision synthetic Wayland captures were inspected. Screenshot export targets the composer window for compose/compose-close scenarios. No real mailbox writes or SMTP submissions were performed.

- `compose-light.png`: independent composer, light appearance.
- `compose-dark.png`: independent composer, dark appearance.
- `compose-close.png`: Save / Don’t Save / Cancel decision.

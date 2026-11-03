# Independent mail composer · 2026-11-03

The composer now opens as a separate nonmodal window with close/minimize/maximize controls and resize handles. Its content layout follows the supplied macOS Mail reference: a compact top action strip, full-width address/subject rows, a horizontal format strip, and an expanding unboxed editor. Window shape/decorations follow the inbox NextKde contract, using a flush rectangular content surface and canonical WindowChrome/MailWindowControls with compositor takeover enabled. Floating header controls use shared liquid actions backed by a same-window LiquidBackdrop; embedded format controls and confirmation interiors use ordinary material. The existing canonical Kos.Ui dependency remains pinned and unchanged.

Closing the composer offers Save / Don’t Save / Cancel. Save failure keeps the content available. Don’t Save deletes the local draft and stops autosave. Active or pending sends block close/discard; accepted sends close only the matching draft. Switching accounts disables editing until the original account is active again. Local drafts also expose a confirmed trash action in the list.

Formatting supports font family/size, color, bold, italic, underline, strikethrough and bullet lists. TextSelection handles selected text and insertion formatting; list operations receive actual editor selection coordinates. Plain draft newlines are preserved when loading the rich editor.

Validation: the user explicitly authorized host testing for this iteration. Host build passed; UI (42 cases), i18n and catalog checks passed, 3/3 in 15.76s. Chinese light/dark composer and close-decision synthetic Wayland captures were inspected. Screenshot export targets the composer window for compose/compose-close scenarios. No real mailbox writes or SMTP submissions were performed.

- `compose-light.png`: independent composer, light appearance.
- `compose-dark.png`: independent composer, dark appearance.
- `compose-close.png`: Save / Don’t Save / Cancel decision.

The original screenshots in this folder record the initial composer iteration. The corrected NextKde-conforming window and close-decision captures are in `../2026-11-03-composer-contract/`. Application screenshot exports capture client pixels; compositor-owned window corners/decorations are not baked into those images.

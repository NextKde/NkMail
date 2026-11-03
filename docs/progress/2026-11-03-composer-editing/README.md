# Composer editing tools · 2026-11-03

The reference guides content and editing actions. NextKde owns window corners and decorations through the existing cooperative WindowChrome/MailWindowControls adapter. The send action sits 10px before the visible first traffic-light circle, sharing its vertical center, as the inbox search action does.

Header icons share 16px SVG slots and a consistent stroke. Undo/redo, formatting, emoji, selected-text links, attachment, Bcc and sending use the same sizing. Formatting/Bcc launchers no longer show checked/active fills. The floating header retains the shared liquid material and explicit same-window backdrop.

Embedded format controls are ordinary solid material: 24px small button groups and 6px corners, with body/title/heading presets, installed fonts, faces, size/color, emphasis, three alignments, bullet/numbered lists and bounded indentation. All four selects use canonical LiquidSelect with materialForm and popupMaterialForm enabled. No custom select background or popup is supplied. Public keyboard navigation and selection remain intact; decorative focus color is suppressed without disabling focus.

The anchored emoji picker uses centered 22px Noto Color Emoji glyphs without text-button padding or elision. All 18 choices fit inside the panel. Emoji insertion replaces the selection in one undo step. The link editor accepts selected-text HTTP(S)/mailto links. Existing draft/send/account guards remain. Apple Siri and Hide My Email are outside this implementation.

Validation: development-VM build and UI/i18n/catalog CTest passed 3/3 in 40.83s, including 47 passing UI cases. Regressions exercise public select opening and actual font/face/size changes, header formatting without checked state, all emoji cell bounds, insertion/closing, selected-text links, unsafe schemes, lists, alignment/indentation persistence and undo, and send-control spacing at four widths.

The user explicitly requested host compilation/preview in this chat. The latest host build and native Wayland light/dark, opened emoji and opened font-menu captures were inspected. Automated checks remained VM-only. The interactive preview is replaced rather than accumulating demo instances and now directly opens the requested synthetic compose scenario. No real mailbox operations were performed. Exports omit compositor-owned corners/buttons and cannot independently validate compositor decoration.

- `compose-light.png`: latest native Wayland Chinese synthetic composer.
- `compose-dark.png`: latest native Wayland dark synthetic composer.
- `compose-emoji.png`: opened emoji picker with all 18 glyphs visible.

Font-menu review stays in temporary local artifacts; no local configuration or raw logs are published. The canonical component library gitlink is unchanged.

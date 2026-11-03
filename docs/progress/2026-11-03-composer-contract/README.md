# Composer NextKde contract correction · 2026-11-03

The composer retains the independent-window workflow and content layout, while following the inbox window/material ownership contract. Its content planes are rectangular and flush: no application-owned window corner radius, window border, clipping mask or decoration shadow. WindowChrome/MailWindowControls preserve the cooperative compositor handoff instead of forcing platformComponent to null or using a separate decoration layout.

Floating header actions use canonical liquid controls with an explicit same-window LiquidBackdrop. Embedded format controls stay ordinary. The close decision uses the canonical LiquidDialog material defaults and 30px LiquidDialogButton actions, without a forced opaque mode or custom destructive tint. The shared library gitlink and platform modules remain unchanged.

Host build and UI/i18n/catalog checks passed (3/3, 15.89s; 43 UI cases). Regression checks cover rectangular client content, enabled platform component, and the floating-action backdrop. Existing cooperative takeover behavior is covered by injected platform controls. Chinese synthetic Wayland composer and close-decision exports were inspected with no runtime warnings. Client screenshot exports do not include compositor-owned window corners or controls and therefore cannot independently establish compositor decoration rendering.

- `compose.png`: corrected client layout and liquid header controls.
- `compose-close.png`: canonical close confirmation.

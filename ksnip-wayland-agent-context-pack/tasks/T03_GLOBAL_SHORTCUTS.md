# T03 — Add Wayland global shortcuts through XDG portal

## Goal

Make built-in capture shortcuts work globally while KSnip is resident.

## Required IDs

```text
capture.rect_area
capture.full_screen
capture.current_screen
capture.active_window
capture.select_window
```

## Constraints

- One portal session per KSnip process.
- Stable IDs.
- Use configured key sequences as preferred triggers when conversion is safe.
- Directly emit existing high-level capture signals.
- Do not emulate raw native key events.
- No polling.
- Recreate session after shortcut settings change.
- Custom user actions are out of scope.

## Acceptance

With another application focused, every mandatory shortcut triggers exactly
one matching KSnip capture action. Restarting KSnip does not duplicate
activation.

# T02 — Connect KSnip area selection to KWin CaptureArea

## Goal

Use the existing KSnip Wayland overlay and then ask KWin to capture exactly the
selected logical rectangle.

## Constraints

- Transparent live overlay; no frozen full-workspace pre-capture.
- No portal picker.
- No whole-screen capture followed by crop.
- Escape cancels.
- No arbitrary sleeps.
- The overlay must be gone before the captured frame is finalized.
- Validate coordinates and empty rectangles.

## Acceptance

- Area shortcut/action shows KSnip selector.
- Result matches the selected area on the target display.
- Twenty exploratory captures and thirty final repeated captures do not hang.
- No empty image is emitted as success.

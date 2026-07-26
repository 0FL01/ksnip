# T01 — Replace the legacy KDE capture transport

## Goal

Implement native ScreenShot2 capture, excluding rectangular area.

## Required modes

- FullScreen -> CaptureWorkspace
- CurrentScreen -> CaptureActiveScreen
- ActiveWindow -> CaptureActiveWindow
- WindowUnderCursor -> CaptureInteractive(0)

## Constraints

- Async D-Bus.
- Blocking pipe, worker-thread read.
- Finite timeouts.
- Strict raw metadata validation.
- Correct descriptor ownership.
- Cancellation is not an error.
- Existing generic portal fallback remains intact.
- No hotkey work in this task.

## Acceptance

Each required mode produces a non-empty image from the installed or otherwise
authorized development build. Interactive selection cancels cleanly.

# T06 — Offline RU/EN area OCR

## Goal

Add one built-in workflow that opens the existing RectArea selector from a
global shortcut, recognizes upright printed Russian and English text locally,
and writes non-empty plain text directly to the clipboard.

## Execution order

1. Prove that the pinned Paddle detector and recognizer can be converted to
   ONNX without patches or custom operators and preserve reference output.
2. Prove one static CPU inference closure in a standalone harness. Only if the
   Paddle route fails a gate, benchmark Tesseract as the single fallback.
3. Add a fake-recognizer vertical slice that routes RectArea capture to text
   clipboard without entering the editor pipeline.
4. Integrate the one selected recognizer and embedded model resources.
5. Add the built-in shortcut to the existing portal session and settings.
6. Build locally and run focused, regression, offline, and installed checks.

Do not build two production engines. Stop at the first failed gate and record
the exact evidence before changing engine or expanding the implementation.

## Engine gates

- Both pinned Paddle 3 PIR models convert with a released Paddle2ONNX tool
  without source patches, custom ONNX Runtime kernels, or lost dynamic shapes.
- Paddle and ONNX Runtime agree on tensor shapes, decoded text, and final
  detector/recognizer output for the frozen RU, EN, and mixed fixtures.
- A standalone reduced-operator CPU build can load both models from memory and
  stays within the product limits fixed before production integration.
- Model revisions, input hashes, conversion command and tool versions, output
  hashes, dictionary order, and redistribution licenses are recorded.

## Workflow contract

- OCR starts only when the shared image grabber has no pending capture.
- OCR exclusively owns the grabber until its RectArea capture finishes or is
  canceled. Ordinary captures may resume while recognition runs; another OCR
  request is ignored until recognition finishes.
- Use the existing visibility adjustment, selector, frozen KDE background, and
  crop. OCR requests use no user delay and exclude the cursor.
- Detach a `QImage` on the GUI thread, run recognition off the GUI thread, and
  call `IClipboard::setText()` only for a non-empty current result.
- Never enter the editor, auto-save, or image-clipboard pipeline. Escape,
  capture failure, OCR failure, and empty output preserve the clipboard.
- Keep the existing plugin-based editor OCR unchanged.

## Packaging contract

- Select exactly one in-process CPU recognizer; no daemon, subprocess, runtime
  download, or system OCR runtime package is allowed.
- Convert models outside the RPM build. The source package receives immutable
  final artifacts plus provenance and hashes and then builds without network.
- Link uncompressed model resources only to the production `ksnip` target, not
  the shared production source list used by test executables.
- The binary package contains no external model files and ships all applicable
  licenses and notices for the linked dependency closure and model data.

## Non-goals

- Additional languages, handwriting, rotated or vertical text, tables,
  columns, document-layout reconstruction, GPU/NPU inference, engine selection,
  OCR history, result editing, progress/cancel UI, and queued OCR jobs.
- A new capture mode, selector, image backend, custom-action framework, second
  GlobalShortcuts session, or redesign of the existing editor OCR plugin.
- A mixed-DPI capture redesign or a custom crosshair on portal-only Wayland.

## Acceptance

- One configured OCR shortcut produces one selector, one recognition job, and
  at most one plain-text clipboard write.
- Successful OCR does not open or modify the editor, save an image, or place an
  image in the clipboard.
- Russian, English, mixed Cyrillic/Latin, punctuation, and multiline fixtures
  pass the frozen quality thresholds and normalization contract.
- Escape, capture/OCR error, empty text, repeated activation, and destruction
  leave the clipboard and application state safe.
- Model loading and inference do not block the GUI thread.
- The installed build recognizes the fixtures with network disabled, an empty
  home directory, and no system Paddle, ONNX Runtime, OpenCV, or Tesseract
  runtime dependency or external model file.
- Existing capture flows, five portal shortcuts, and the automated Qt 6 suite
  continue to pass.

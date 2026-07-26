# T04 — User-local installation and desktop identity

## Goal

Make ScreenShot2 authorization and portal application identity reliable
without touching system files.

## Required artifacts

- user-local absolute executable path;
- generated desktop entry with matching `Exec`;
- restricted-interface declaration;
- symlink or wrapper for convenient CLI use;
- safe uninstaller.

## Constraints

- No sudo.
- No writes outside `~/.local`.
- Back up a pre-existing user desktop entry before replacement.
- Do not overwrite the distribution binary.
- Test by launching through the installed desktop entry.

## Acceptance

Native ScreenShot2 capture succeeds from the installed launcher and after a
logout-free application restart where possible.

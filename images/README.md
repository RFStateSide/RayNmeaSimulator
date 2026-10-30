# Application icon assets

The owner-approved **Glossy Wireless Signal Ship Icon.png** supplied in the M2.21 transparency acceptance correction
is the authoritative original artwork, preserved byte-for-byte (1254 × 1254 RGBA,
transparent background). Source SHA-256:
`8c95b33c3436cabe956430369aebe3604cae97b06934483ba71997bd3182ca75`.

The owner supplied and approved this image for application use. No third-party
license or independent authorship claim is inferred. Historical milestone reports
retain their original artwork descriptions; their old source and unused 64px
resource are removed from the current application asset set.

Regenerate from any working directory with:

```sh
python scripts/generate-app-icons.py
```

Pillow is a development asset tool only; no new application dependency is added.
The source stays byte-for-byte intact. Derivatives trim to the visible alpha
bounds (alpha >= 16, avoiding faint export speckles), preserve aspect ratio, and
use the smallest enclosing transparent square. There is no decorative padding,
colored background, recoloring or stretching. LANCZOS resampling is used for PNG
and ICNS representations. Generated assets remain checked in.

| Asset | Use |
| --- | --- |
| `Glossy Wireless Signal Ship Icon.png` | Approved original source |
| `RayNmeaSimulator_1024.png` | Qt resource and Linux desktop icon |
| `RayNmeaSimulator.ico` | Windows executable: 16, 24, 32, 48, 64, 128, 256 px |
| `RayNmeaSimulator.icns` | macOS packaging representations through 1024 px |

## Theme and window behavior

`DesktopIcons::forPalette` now returns the same transparent artwork in every
theme, following the owner's correction. The former light rounded Dark-theme
backing is removed. MainWindow, Setup and secondary-window icon inheritance,
theme reload/cancel handling, and text-only Layout tabs remain unchanged.

Windows executable/shell icons and Linux launchers use the static derivative.
Window managers may prefer that desktop entry over the runtime window icon.
Shell caches/pinned shortcuts can outlive a running process; the application does
not clear OS caches or change desktop preferences. qmake's existing explicit
MinGW ICO → resource-object dependency keeps normal incremental builds current.

Linux packaging installs the unchanged PNG in `/usr/share/pixmaps`, with an
explicit desktop-entry icon path. This avoids hicolor themes that do not index
`1024x1024/apps`. Qt Creator development builds can use the user-level registration
helper described in [the Linux guide](../docs/build-linux.md#gnome-dock-and-application-switcher-registration).

## macOS support boundary

The generated ICNS and existing `macx: ICON` project reference remain supported
asset inputs. The pre-existing Windows/Linux backend guard remains unchanged.
Native macOS build/bundle, Dock, signing and runtime qualification are deferred
under Issue #6; asset generation does not claim a completed macOS port.

## Owner provenance attestation — M2.50

On 2026-10-28 the owner confirmed: “ChatGPT created boat logo for me.” This applies
to the supplied boat design documented above. The PNG/ICO/ICNS are reproducible
derivatives of that source via generate-app-icons.py. Record this as owner-directed
AI-generated artwork approved for application use; do not infer exclusive
copyright, third-party certification or universal rights clearance from visual
approval alone. The authoritative original hash above remains unchanged.

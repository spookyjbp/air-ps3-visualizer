# AIR PS3 Visualizer v1.0.0

First release of AIR PS3 Visualizer: the AIR PS3 AirPlay 1 receiver with an
animated, music-reactive interface.

## New

- Now-playing screen: title (wraps so long names show in full), artist and
  album, progress bar, and status (Ready / Connecting / Playing / Paused).
- Nine visualizers: Spectrum, Ribbons, Halo, Scope, Nebula, plus the trippy
  full-screen modes Feedback (MilkDrop-style), Tunnel, Kaleidoscope and Plasma.
- Six themes: Aurora, Sunset, Silver, Ocean, Neon, Ember.
- Volume and mode/theme overlays; choices are remembered.
- Installs as its own app (`AIRPLAY03`), alongside the original AIR PS3.
- New XMB icon; no XMB background image.

## Under the hood

- The interface runs on its own thread below the audio thread, so a slow
  frame lowers the frame rate rather than interrupting audio. If it fails,
  the app falls back to a static screen.
- Frames are scaled to the TV's resolution by the PS3's graphics chip.
- Track text and progress are read from the iPhone (`md=0,2`); artwork isn't
  supported.

## Known limits

- Frame rate on hardware is still being tuned; the log records frame timing
  (`VISUAL_*` lines in `/dev_hdd0/tmp/air-ps3-visualizer.log`).
- Same receiver limits as AIR PS3: AirPlay 1 audio only, foreground app,
  custom firmware required.

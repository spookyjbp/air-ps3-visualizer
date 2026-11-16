# AIR PS3 — Visualizer Edition

> **This is a fork of [MKWorldWide/ps3-airplay](https://github.com/MKWorldWide/ps3-airplay)**
> that adds an animated, music-reactive interface. Audio streaming is the same
> code as upstream.

![Spectrum visualizer with now-playing info](docs/images/visual-spectrum.jpg)

**What's new in this fork**

- **Clean now-playing screen** — track title, artist · album, progress with
  elapsed/total time, and a status pill (Ready / Connecting / Playing / Paused).
- **Five visualizers** — Spectrum, Ribbons (XMB-style waves), Halo, Scope and
  Nebula, all driven by a real-time FFT of the stream with beat detection.
- **Six themes** — Aurora, Sunset, Silver, Ocean, Neon, Ember.
- **Overlays** — volume indicator when you change volume on the phone, and a
  short label when you switch modes or themes.
- **Controller** — ◀ ▶ / L1 R1 change visualizer, △ changes theme, □ hides the
  info panel, START exits. Your choices are remembered.

| | | |
| --- | --- | --- |
| ![Ribbons](docs/images/visual-ribbons.jpg) | ![Halo](docs/images/visual-halo.jpg) | ![Nebula](docs/images/visual-nebula.jpg) |

Details, architecture and tuning: **[Visual UI](docs/VISUAL_UI.md)**.

> **Status:** the UI is covered by host tests and a mocked render-thread test,
> but this fork has not been run on PS3 hardware yet. It uses the same title ID
> (`AIRPLAY02`), so installing it replaces upstream AIR PS3 on the console.
> There's no prebuilt PKG for this fork yet; build it with
> [Build from Source](docs/BUILD_FROM_SOURCE.md). The download links further
> down are upstream v1.0.0, which doesn't include the new UI.

---

![AIR PS3 — silver XMB-inspired waves with the AirPlay symbol](assets/air-ps3/PIC1.PNG)

**Your PlayStation 3, now an AirPlay speaker.**

AIR PS3 lets you stream audio from your iPhone to a PlayStation 3 over your local network. Choose **AirPS3** in the iPhone's AirPlay output menu, and the console handles the sound. Your music app keeps running on the phone—this is an AirPlay 1 audio receiver, not Spotify Connect.

**Latest release:** v1.0.0 · **XMB name:** AIR PS3 · **AirPlay destination:** AirPS3

Developed and maintained by **[EsKaye](https://github.com/EsKaye)** for [MKWorldWide](https://github.com/MKWorldWide).

## Download

**[Download AIR PS3 v1.0.0 from GitHub Releases](https://github.com/MKWorldWide/ps3-airplay/releases/latest)**

[Direct PKG download](https://github.com/MKWorldWide/ps3-airplay/releases/download/v1.0.0/ps3-airplay-receiver-1.0.0.pkg) · [SHA-256 checksums](https://github.com/MKWorldWide/ps3-airplay/releases/download/v1.0.0/SHA256SUMS.txt)

This is a **type-1 homebrew/debug PKG** for compatible PS3 custom firmware. **Tested on Evilnat 4.93 CFW (Cobra 8.5).** Other firmware configurations haven't been verified, and retail firmware isn't supported.

## Install and play

1. Download and install the PKG using your PS3's XMB Package Manager.
2. Launch **AIR PS3** from the **Game** category.
3. On your iPhone, open the AirPlay audio-output menu and select **AirPS3**.
4. Enjoy the music. **Press START on your PS3 controller to exit to the XMB.**

For checksums, troubleshooting, removal, and rollback, see the [installation guide](docs/INSTALLATION.md).

To verify the PKG without downloading the separate build-evidence file, put the PKG and `SHA256SUMS.txt` in one folder and run:

**macOS**
```sh
grep '  ps3-airplay-receiver-1.0.0.pkg$' SHA256SUMS.txt | shasum -a 256 -c -
```

**Linux**
```sh
grep '  ps3-airplay-receiver-1.0.0.pkg$' SHA256SUMS.txt | sha256sum -c -
```

## What's included

AIR PS3 handles AirPlay 1 discovery, encrypted RAOP/ALAC audio streams, audio buffering, and playback through the PS3. It includes the approved AIR PS3 artwork and clean START-button exit.

The project has been exercised on real hardware, including **Evilnat 4.93 CFW**. Earlier development testing included extended playback and XMB open/close cycles; the initial release still has a possible brief hitch when returning from the PS-button menu. See the [test matrix](docs/TEST_MATRIX.md) for measured results and limits.

AIR PS3 runs in the foreground. It doesn't currently support AirPlay video, Spotify Connect, background game-audio mixing, or resident playback; those are separate projects or possible future work.

## Build it yourself

Want to inspect the source, run the same checks, or produce your own PKG? Start with **[Build from Source](docs/BUILD_FROM_SOURCE.md)**. It documents the Linux x86_64 / Lima environment, the pinned PS3DK and PSL1GHT toolchain, Mbed TLS, exact build commands, and how to validate your output.

[Development documentation](docs/README.md) · [Roadmap](docs/ROADMAP.md) · [Contributing](CONTRIBUTING.md)

## License

AIR PS3's project code is available under the [MIT License](LICENSE). Third-party components and artwork have their own notices; see [Third-party notices](THIRD_PARTY_NOTICES.md).

## ❤️ Support development

If AIR PS3 gave your old PlayStation another reason to stay plugged in, consider [sponsoring my next experiment](https://github.com/sponsors/EsKaye). Your support helps me build, test, and maintain unusual homebrew projects.

**AIR PS3 is free. Sponsorship is always optional.**

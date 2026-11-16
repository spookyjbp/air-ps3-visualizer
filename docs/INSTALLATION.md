# Install and use AIR PS3 Visualizer

**AIR PS3 Visualizer** is an AirPlay 1 audio receiver with an animated music
visualizer, for a PlayStation 3 running compatible custom firmware. The
receiver comes from [AIR PS3](https://github.com/MKWorldWide/ps3-airplay),
which was tested on **Evilnat 4.93 CFW** (Cobra 8.5). It's a foreground app,
not a background music plugin.

## Download and verify

1. Open the [Releases page](https://github.com/spookyjbp/air-ps3-visualizer/releases).
2. Download `air-ps3-visualizer-<version>.pkg` and `SHA256SUMS.txt` into the
   same folder.
3. Optionally check the download:

**macOS**
```sh
shasum -a 256 -c SHA256SUMS.txt
```

**Linux**
```sh
sha256sum -c SHA256SUMS.txt
```

The package is a type-1 debug/homebrew PKG for compatible **CFW**
installations; retail firmware isn't supported. No firmware updates or boot
plugins are needed.

## Install and play

1. Copy the PKG to a USB drive and install it with the XMB **Package
   Manager**.
2. From the XMB **Game** column, launch **AIR PS3 Visualizer**.
3. On your iPhone, open the AirPlay audio menu and choose **AirPS3**.
4. Play music. Your music app keeps running **on the iPhone**; the PS3
   receives its AirPlay 1 audio stream.

**To exit, press START on your PS3 controller.** Wait until you're back at
the XMB before relaunching. See the [README](../README.md#controls) for the
visualizer controls.

## What to expect

- It installs as its own app (`AIRPLAY03`) next to the original AIR PS3 (`AIRPLAY02`). Run one at a time: both appear on the iPhone as **AirPS3**.
- The PS button opens the system menu; a brief hitch on returning to the app has been observed in earlier hardware testing.
- This release does **not** mirror video, run Spotify Connect, mix with game audio, or load a resident background-audio SPRX.
- Compatibility beyond the tested Evilnat 4.93 CFW environment is not guaranteed.

See [testing and known boundaries](TEST_MATRIX.md) for more technical details.

## Remove, roll back, or recover

To remove the app, delete **AIR PS3 Visualizer** from the XMB using the standard game/app removal controls. Keep any known-good earlier package and its hash if you want to roll back; you may need to remove the current installation before installing an older build.

If the app is responsive, **press START to exit**. If controller input stops working but the console is still reachable, an existing, authorized webMAN exit method may work. If the whole console is unresponsive, use its normal power-button recovery procedure rather than modifying boot plugins.

The local diagnostic log is stored at `/dev_hdd0/tmp/air-ps3-visualizer.log`. It can contain device/runtime details. Save a private copy for troubleshooting and redact it before sharing excerpts publicly.

## Building from source

For a Linux x86_64 build using PS3DK v0.27.0, PSL1GHT, Mbed TLS and the checked release tag, follow [Build from Source](BUILD_FROM_SOURCE.md). Build/package validation does not guarantee compatibility with a physical PS3.

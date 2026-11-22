# DDR Mini null-driver bootstrap

This directory cross-compiles OpenITG for the DDR Mini's AArch64 userspace.
The target uses the DDR Mini firmware's SDL2 video backend with an OpenGL ES
1.1 renderer.  Null display remains available as a fallback.  Ogg Vorbis
decoding and ALSA sound are enabled, with the null sound driver retained as a
fallback; movie playback still uses its null driver.

Build from the repository root:

```sh
Tools/ddrmini/docker-build.sh
```

Deploy the complete layered game assets to a mounted USB root:

```sh
Tools/ddrmini/deploy-usb.sh /path/to/usb
```

The script does not deploy the executable, launcher, or libraries.  It
preserves `openitg/.openitg`, including preferences, profiles, logs, and other
writable data.

Set `DDRMINI_BUILD_JOBS=1` on a memory-constrained build host.  The output is
written to `Build/ddrmini-openitg/openitg`, with the complete ELF version report
in `Build/ddrmini-openitg/version-info.txt`.  The output directory also contains
the `DDRmenu` launcher used by the firmware's USB loader.  Keep the unstripped
`openitg.debug` file on the build host for resolving crash addresses; it does
not need to be deployed.

The container copies the source into temporary storage before building.  It
does not modify the repository or include it in the Docker image.

This binary defaults to:

- 1280x720 fullscreen SDL2/OpenGL ES 1.1, with null fallback
- ALSA software-mixed sound, with ALSA and null fallbacks
- null movie playback
- SDL2 USB keyboard input through OpenITG's normal keyboard mappings

It is still a bring-up artifact.  Movie playback and cabinet-pad input remain
disabled while the SDL keyboard and device ALSA paths are validated on hardware.

The DDR Mini lights driver controls the twelve cabinet sysfs LEDs and sends
five-byte reports to each `33dd:0014` pad's correlated hidraw output.  It falls
back to `/dev/input/hidraw_p1` and `hidraw_p2` when topology discovery finds no
pads, avoids redundant writes, and turns all outputs off during clean shutdown.

For an on-device bootstrap, copy `DDRmenu` to the USB root and place the binary
and the normal OpenITG data directories under `USB root/openitg/`.  Logs are
written to `USB root/openitg/Logs/`.  Copy the generated `lib/` directory next
to the binary; it contains the ARM64 third-party libraries that are not assumed
to exist in the firmware.  It intentionally excludes `libstdc++.so.6` and
`libgcc_s.so.1`: the firmware's Mali GLES library requires the newer C++ runtime
provided by the device.

OpenITG does not support the newer StepMania `Portable.ini` convention.  The
launcher sets `HOME` to the USB application directory, causing OpenITG's normal
Unix writable overlay to live at `openitg/.openitg/` instead of the firmware's
read-only `/.openitg`.

The launcher tracks `gpio-384` as the power switch, gives OpenITG five seconds
to shut down cleanly, stops its audio routing/volume monitor, and then returns
control to the firmware for normal power-off.  While the game runs, the monitor
keeps speaker/headphone routing and the physical volume knob synchronized.

On ARM64, fatal signals produce timestamped reports in
`openitg/Logs/crashinfo-*.txt`.  The launcher also directs `TMPDIR`, `TMP`, and
`TEMP` to `openitg/Logs/tmp`, enables kernel core dumps where permitted, and
records the firmware's `core_pattern` in `ddrmini-launcher.log`.  Relative core
patterns place a fallback core in `openitg/`; absolute or piped patterns remain
under firmware control.

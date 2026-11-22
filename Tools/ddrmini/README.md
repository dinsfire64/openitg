# DDR Mini null-driver bootstrap

This directory cross-compiles OpenITG for the DDR Mini's AArch64 userspace.
The target uses the DDR Mini firmware's SDL2 video backend with an OpenGL ES
1.1 renderer.  Null display remains available as a fallback while sound and
movie playback still use their null drivers.

Build from the repository root:

```sh
Tools/ddrmini/docker-build.sh
```

Set `DDRMINI_BUILD_JOBS=1` on a memory-constrained build host.  The output is
written to `Build/ddrmini-openitg/openitg`, with the complete ELF version report
in `Build/ddrmini-openitg/version-info.txt`.  The output directory also contains
the `DDRmenu` launcher used by the firmware's USB loader.

The container copies the source into temporary storage before building.  It
does not modify the repository or include it in the Docker image.

This binary defaults to:

- 1280x720 fullscreen SDL2/OpenGL ES 1.1, with null fallback
- null sound
- null movie playback
- the inert `MonkeyKeyboard` input handler

It is only a bring-up artifact.  A production DDR Mini build will replace
these defaults with the device-specific SDL2/EGL/GLES, audio, and input paths.

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

# DDR Mini null-driver bootstrap

This directory cross-compiles OpenITG for the DDR Mini's AArch64 userspace.
The first milestone deliberately uses the null display, sound, and movie
drivers.  It proves that the old OpenITG core can configure, link, and start on
the device before SDL2, EGL/GLES, and audio are introduced.

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

- 1280x720 with the null renderer
- null sound
- null movie playback
- the inert `MonkeyKeyboard` input handler

It is only a bring-up artifact.  A production DDR Mini build will replace
these defaults with the device-specific SDL2/EGL/GLES, audio, and input paths.

For an on-device bootstrap, copy `DDRmenu` to the USB root and place the binary
and the normal OpenITG data directories under `USB root/openitg/`.  Logs are
written to `USB root/openitg/Logs/`.  Copy the generated `lib/` directory next
to the binary; it contains the ARM64 third-party libraries that are not assumed
to exist in the firmware.

OpenITG does not support the newer StepMania `Portable.ini` convention.  The
launcher sets `HOME` to the USB application directory, causing OpenITG's normal
Unix writable overlay to live at `openitg/.openitg/` instead of the firmware's
read-only `/.openitg`.

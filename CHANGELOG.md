# Changelog

## 1.1.2 (2026-11)
- **CPU figure recorded.** Measured on an MPC One with the Open MPC bench: **PASS**, 5.1% of an audio block at the 99th percentile and 5.9% at worst, so it runs comfortably alongside a full project. It shows on the catalog card and in INSTALL.md.
- Nothing else changed: same sound, controls and saved projects as 1.1.1.

## 1.1.1 (2026-11)
- **Ready for MPC OS 3.10.1's plugin folders.** Every download now carries the `plugin.json` that MPC OS 3.10.1 looks for, so the plugin folder can be copied by hand straight into `VST2 Plugins`.
- **Gen2 MPCs in the Open MPC catalog.** A separate 64-bit download (`-mpc-aarch64.zip`), so the Plugin Manager can install it on Gen2 MPCs, and the catalog's 3.10.1 download (`-mpc-sdk.zip`).
- Built with the current Open MPC kit. Sound, controls and saved projects are unchanged.

## 1.1.0 (2026-11)
- **Three looks.** A new default look, **Print Plate** (a warm printed panel), plus **Night Rack** (dark rack unit) and **Colour Block** (flat, big type), each in PotentialPatina's own colour. Pick one with the big LOOK keys on the SETTINGS page; it shows the next time the plugin loads (take it off and add it again, or load your project again), and it is remembered on your MPC for every project.
- Sound, controls and saved projects are unchanged. One parameter was added at the end of the list (LOOK); it is never saved in a project.
- **Akai's plugin folder format** for MPC OS 3.10.1 and later: a second download (`-akai-vst2.zip`) with 32-bit and 64-bit builds, so it now also runs on Gen2 MPCs. Copy the folder into `VST2 Plugins` and turn on External Plugin Support: no hacking needed.

## 1.0.4 (2026-11)
- The plugin's name on its page now reads **PotentialPatina** (was all capitals), matching its name in the MPC's plugin list. Nothing else changed.

## 1.0.3 (2026-11)
- New look: Barlow fonts, raised panels with soft shadows, glowing amber knob arcs, even-sized button labels and darker display wells.

## 1.0.2 (2026-11)
- First public release.
- Noise, Wobble, Drive, Digital, Tone and Space sections with a MAGNITUDE knob.
- 9 presets, set subtle.
- Moved to its own repository (mpc-vst-potentialpatina); the code is the same as in the earlier `potential-plugins` repo.

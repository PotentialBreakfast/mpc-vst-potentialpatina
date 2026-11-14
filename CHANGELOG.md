# Changelog

## 1.5.0 (2026-11)
- **USER presets.** A new USER page with 8 slots: tap **SAVE**, then a slot, to keep the sound you have now; tap a slot to bring it back. Filled slots read USER 1 to USER 8, empty ones EMPTY, and the slot you loaded last is lit. No typing.
- The slots are kept in a file beside your VST2 Plugins folder (`PotentialSuite Presets/PotentialPatina.txt`), so every project and every copy of the plugin sees the same slots, and updating the plugin keeps them.
- Use the same slot number in PotentialPunch, PotentialHeat, PotentialGrit and PotentialBleed to keep a chain of your own.
- **MANUAL page.** The plugin's manual is now on the MPC's screen: tap **MANUAL**, then the page buttons along the top (what it does, the controls, presets, the FX chains and their insert order, USER presets). The full PotentialSuite manual is on the [PotentialSuite page](https://github.com/PotentialBreakfast/potential-plugins).
- Ten parameters were added at the end of the list (Save User Preset, User 1 to User 8, Manual Page); saved projects open as before.

## 1.4.0 (2026-11)
- FX chain presets: Dusty Loop, 90s Stab, Break Bus and Dub Drums, in the MPC's PRESET menu. Pick the same name in PotentialPunch, PotentialGrit and PotentialBleed to load the whole chain (see the PotentialSuite page).

## 1.3.0 (2026-11)
- **ERA**: one tap sets all six sections for a sound of its time: Patina, '50s 78, '60s 45, '70s Tape, '80s Sampler, '90s Cassette, AM Radio and VHS. They're in the MPC's PRESET menu too.
- **AGE**: clean to fully worn in one move. As it rises, the sections come in one after another (tone first, then drive, noise, wobble, and digital and space last). 50 % is the sound exactly as set, so projects from before sound the same; above that everything keeps going, well past it.
- **DUCK** (NOISE): turns the noise down while the music plays (up to about 26 dB), so the crackle sits under the beat and comes up in the gaps.
- The six sections are a little shorter to make room for the ERA strip along the bottom. Three parameters were added at the end of the list (Era, Age, Noise Duck); saved projects open as before.

## 1.2.0 (2026-11)
- **ROLL**: one tap switches on a random set of sections with random settings, for a new kind of old sound each time. **MILD** keeps it subtle (no digital crunch or room), **MEDIUM** may add them, **WILD** turns most things on and up. OUTPUT is left alone. Keep rolling until you like it, then fine-tune.
- MAGNITUDE, MIX and OUTPUT moved up a little to make room for it. Two parameters were added at the end of the list (Roll, Roll Wildness); saved projects open as before.

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

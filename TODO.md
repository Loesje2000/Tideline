# Tideline TODO

## Completed

- [x] Confirm macOS Standalone, AU, and VST3 project targets.
- [x] Add ten supported streaming-service/mode entries.
- [x] Implement basic per-service penalty and preview-gain rules.
- [x] Preserve maximum true peak for a measurement.
- [x] Build clickable all-services penalty grid.
- [x] Build source/normalized loudness block view.
- [x] Support dropping multiple files and folders in Standalone.
- [x] Add an album queue with previous/next navigation and track counter.
- [x] Add asynchronous offline per-track album measurement.
- [x] Calculate TIDAL's album-wide gain from the loudest track.
- [x] Build and codesign-verify the Debug Standalone app.

## Next: test and refine

- [ ] Test folder drops with a real multi-track album, including WAV, AIFF, FLAC, MP3, AAC/M4A, and unsupported files.
- [ ] Verify offline results against a trusted loudness meter on several tracks.
- [ ] Check Spotify Normal/Quiet peak-safe boost behavior using quiet, low-peak material.
- [ ] Check each penalty-grid selection and preview transition in the Standalone app.
- [ ] Add a scrollable per-track album results table, with export/copy options.

## Streaming-mode features still to build

- [ ] Spotify Loud limiter emulation: -1 dBFS threshold, 5 ms attack, 100 ms release.
- [ ] Spotify Loud limiter gain-reduction display and delta/difference monitor.
- [ ] Explicit Preview refresh control and visible stale-gain warning; expose Auto Gain in the UI.
- [ ] Lock/freeze measurement control.
- [ ] Host-transport processing option.
- [ ] Preview gain-increase warning when a change exceeds 6 dB.
- [ ] Apple Legacy/Sound Check mode and corresponding calculation rules.
- [ ] TIDAL album-mode UI that applies the calculated album gain to every track result.
- [ ] More complete standards validation for BS.1770-4 / LRA / true-peak behavior.

## UI and product work

- [ ] Improve responsive layout for small and wide Standalone windows.
- [ ] Add mini/list display mode and preset UI sizes.
- [ ] Add tooltips that explain why a penalty is zero, negative, or peak-limited.
- [ ] Add empty, analysis-progress, completed, and error states for the album workspace.
- [ ] Decide whether to persist queue/results in plugin state or keep them Standalone-only.

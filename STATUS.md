# Tideline status

Last updated: 2026-06-21

## Session outcome

Tideline is a macOS AU, VST3, and Standalone loudness-reference tool. The Debug Standalone target builds successfully as a universal arm64/x86_64 app.

This session moved Streaming mode from a single-target meter toward a Loudness Penalty-inspired workflow while retaining Tideline's own visual identity.

### Implemented today

- Added a ten-service streaming model: Spotify Normal/Quiet/Loud, Apple Music, YouTube Video/Music, TIDAL, Amazon Music, Pandora, and Deezer.
- Added service-specific preview-gain rules:
  - YouTube, TIDAL, Amazon, and Deezer do not boost quiet material.
  - Spotify Normal and Quiet restrict boosts to a -1 dBTP maximum.
  - Spotify Loud remains identified as the limiter-based mode; limiter emulation itself is still pending.
  - Apple and Pandora previews use a conservative peak-safe boost.
- Changed true-peak handling to retain the maximum for the current measurement instead of applying a decay.
- Added a streaming penalty grid with click-to-select service cells.
- Added a source/normalized block view showing true peak, integrated loudness, PLR, LRA, and preview gain.
- Expanded standalone file handling:
  - A drop can contain a file, multiple files, or a folder.
  - Folder contents are filtered for supported audio formats and queued in filename order.
  - Previous/next controls, queue position, and measurement reset on track change are available.
- Added background offline album analysis. It measures each queued track's integrated LUFS, true peak, and LRA without requiring real-time playback, and calculates the TIDAL album gain from the loudest measured track.

## Verification

Successful command:

```sh
xcodebuild -project Builds/MacOSX/Tideline.xcodeproj \
  -target 'Tideline - Standalone Plugin' \
  -configuration Debug build CODE_SIGNING_ALLOWED=NO
```

The resulting app is at `Builds/MacOSX/build/Debug/Tideline.app`.

## Notes for next session

- No representative album was run through the new queue/analyser during this session; test it with a real folder before relying on its values.
- `Builds/` and `Naamloos/` were already untracked in the working tree and were intentionally not altered.

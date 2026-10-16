# EchoNX

**Listen to YouTube Music on your Nintendo Switch, directly over Wi-Fi. No PC or phone companion required.**

EchoNX is a native homebrew music player for Switch consoles running custom firmware. Search for a song, build your library, and listen with artwork and lyrics in a fullscreen player.

<p align="center">
  <img src="docs/fullscreen.jpg" width="90%" alt="EchoNX fullscreen music player with album artwork, lyrics, and playback controls">
</p>

## Features

- **Standalone streaming:** music plays directly on the Switch over Wi-Fi.
- **Search and browse:** find songs and open artist or album pages.
- **Your library:** save songs to the SD card and get recommendations based on your saved tracks.
- **Fullscreen player:** blurred artwork background, scrolling titles, and touch playback controls.
- **Lyrics:** animated synced lyrics, plus touch scrolling for lyrics without timestamps.
- **Touch and controller navigation:** swipe lists, or select songs with directional buttons and either analog stick.
- **Playback controls:** queue, pause, previous/next, tap-to-seek, shuffle, and repeat (off, all, or one).
- **Album art mode:** centered artwork with rounded corners, a soft shadow, and a solid background sampled from the cover. Press + to enter or leave and − to toggle the backlight; tap the lit screen to toggle a large bold clock over a blurred backdrop. Battery percentage and a charging indicator are shown across views. Other app inputs are locked.
- **Audio output indicator:** see whether audio is routed to speakers, a headset, or another output.

## What you need

- A Nintendo Switch with custom firmware.
- An SD card and a working Wi-Fi connection.
- The `echonx.nro` application file.

## Quick start

1. Copy `echonx.nro` to `sd:/switch/echonx/echonx.nro`.
2. Connect your Switch to Wi-Fi and launch EchoNX through Homebrew Menu.
3. Press **X** to search, select a song, and press **A** to play.
4. Press **−** to save the current song. Tap its cover to open the fullscreen player.

Your saved library lives at `sd:/switch/echonx/library.json`.

## Good to know

- **No Google sign-in:** recommendations use your locally saved songs, rather than your YouTube Music account history.
- **Playback stays inside EchoNX:** it does not run over other games.
- **Lyrics and album details vary:** some songs have synced lyrics, some have plain lyrics, and others have none.
- **Still in development:** service changes and network interruptions can affect playback. Artwork cache limits and streaming retries are implemented, but long-session stability still needs device testing.

## Session and local playlists

EchoNX restores the last queue, song position, shuffle order, and repeat mode without autoplay. Press Play to continue. Sessions are checkpointed every 10 seconds and saved when leaving the app.

Open **Library → Playlists** to create, rename, play, or delete a local playlist. Select a song and press **X**, then choose **Add to playlist**. The same menu removes songs from an open playlist. Playlist entries still stream over Wi-Fi.

Session and playlist data are stored in `sd:/switch/echonx/session.json` and `sd:/switch/echonx/playlists.json`, with backup files.

Open **Library → History** to revisit the 200 most recently played songs. A replay moves the song to the top; restored sessions and failed connections do not count as plays. History is stored locally in `sd:/switch/echonx/history.json`.

## Screenshots

| Main screen | Fullscreen player | Album art mode |
| --- | --- | --- |
| ![EchoNX main screen](docs/main.jpg) | ![EchoNX fullscreen player](docs/fullscreen.jpg) | ![EchoNX album art mode](docs/album%20art.jpg) |

## Controls

| Input | Action |
| --- | --- |
| A | Play the selected song; in fullscreen, play/pause |
| B | Leave fullscreen or return to Home |
| X | Open song/playlist options |
| Y | Queue the selected song to play next (song lists) |
| Up/down or either analog stick | Move the song selection |
| L / R | Switch tabs |
| ZL / ZR | Previous / next song |
| − | Save/remove the current song; in album art mode, toggle the backlight |
| + | Enter or leave album art mode |
| Right stick click | Exit the app |
| Left stick click | Open the artist page |

**Touchscreen:** swipe the song list or plain lyrics, tap or drag the progress bar to seek, and tap the cover to enter or leave fullscreen. In fullscreen, tap the artist or album name to open its page.

In album art mode, press **−** to turn the backlight off or on while music continues. While the screen is lit, tap to show or hide the clock. Other app controls are ignored. Press **+** to turn the screen on and return to the previous view. This does not put the console to sleep. Artwork follows the playing song, including automatic track changes.

## Persistent cache

Artwork, lyrics, search results, artist and album metadata, and recommendation data are cached under `sd:/switch/echonx/cache/` and reused after restarting the app. The disk cache is limited to 100 MB and removes the least recently accessed files when full.

Artwork is kept for 30 days, lyrics and artist/album metadata for 7 days, and search/recommendation data for 24 hours. Refresh bypasses the metadata cache.

Audio data and temporary streaming URLs are not stored in this cache. Playback still requires an internet connection.

## Building from source

EchoNX uses C++, libnx, SDL2, SDL2_ttf, SDL2_image, FFmpeg, curl, and mbedTLS. The included build script runs in Docker using `devkitpro/devkita64`.

**A Docker portlibs volume containing FFmpeg built for Switch is required.** The script does not build FFmpeg from scratch; the standard devkitPro image alone is not enough.

From PowerShell in the repository directory:

```powershell
powershell -ExecutionPolicy Bypass -File .\Build.ps1
```

The default portlibs volume is `switch-music-portlibs`. To use another volume:

```powershell
.\Build.ps1 -PortlibsVolume your-volume-name
```

The application is written to `build-switch/echonx.nro`. Build output is logged in `build.log`.

## Project layout

| Path | Contents |
| --- | --- |
| `src/` | Application interface, service requests, and audio playback |
| `include/` | Song models, library storage, recommendations, lyrics, and UI helpers |
| `romfs/` | Certificate bundle used for HTTPS |
| `vendor/` | Bundled JSON dependency |
| `tests/` | Library persistence and visual helper tests |

The host tests can be configured independently with CMake from `tests/`. They cover library replacement and recovery, icon geometry, edge smoothing, and UI helper logic. They do not replace playback testing on a Switch.

## Credits

- **Echo Music** — visual inspiration for the player.
- **devkitPro and libnx** — Switch toolchain and system APIs.
- **SDL2 and FFmpeg** — interface rendering and audio playback.
- **LRCLIB** — lyric data.
- **curl, mbedTLS, and nlohmann/json** — networking, TLS, and JSON support.

EchoNX is not affiliated with Nintendo, Google, YouTube, or Echo Music.

## AI-assisted development

EchoNX was developed with AI assistance, including help with implementation, debugging, and documentation. AI-generated code may contain mistakes; testing and review are ongoing.

## License

**GPL-3.0**. See [LICENSE](LICENSE). Third-party components retain their respective licenses.

The clock uses Barlow SemiBold, licensed under the SIL Open Font License; see [font license](romfs/fonts/OFL.txt).

# ffmpeg-static-win

x64-windows-static ffmpeg (libs + headers, vcpkg) for
[StickerProcess](https://github.com/TheFunny/StickerProcess) desktop CI, consumed
as `FFMPEG_DIR`. Unpacking the release asset gives a directory with `lib/`
`include/` `share/`, exactly what the app's `build.rs` +
`ffmpeg-sys-the-third` expect.

## Tags

| tag | built by | contents |
|---|---|---|
| `ffmpeg-static-v1` | local vcpkg (transition) | `ffmpeg-static-x64-win.tar.xz` |
| `ffmpeg-static-v2` | `.github/workflows/build-static.yml` | same file, plus `SHA256SUMS.txt` |

## How it is built

`Build static ffmpeg` (Actions → Run workflow, with a tag) on `windows-latest`:

1. extract the pinned vcpkg tree (`VCPKG_PIN` in the workflow; it is the version
   pin — it decides the ffmpeg/libvpx port versions)
2. `vcpkg install` in **manifest mode** (`vcpkg.json`): ffmpeg with
   `avcodec avdevice avfilter avformat swresample swscale vpx zlib` +
   `libvpx[highbitdepth]`
3. assert the package shape and the resolved ffmpeg version
4. `test/run-smoke.ps1` — compile `test/static_smoke.c` with MSVC against the
   package and assert a real **10-bit VP9 encode** plus **no ffmpeg DLL imports**
5. package `lib/ include/ share/` as `ffmpeg-static-x64-win.tar.xz` + `SHA256SUMS.txt`
6. publish the release, then `verify-app` builds StickerProcess (`cargo build
   --release` with `FFMPEG_DIR` pointing at the published asset) — that job is
   what catches a dependency change breaking the app's static link list

`vcpkg-src/downloads` and the files binary cache are restored via `actions/cache`,
so a rerun only recompiles what actually changed.

### Why manifest mode

The `vpx` feature of vcpkg's ffmpeg port depends on plain `libvpx`; `highbitdepth`
is a separate libvpx feature. Declaring `libvpx[highbitdepth]` next to it resolves
both in one graph. The classic-mode equivalent was
`vcpkg install libvpx[highbitdepth] --triplet x64-windows-static --recurse`,
i.e. install twice — and forgetting it silently drops VP9 10-bit support
(`yuv420p10le` then fails with "Specified pixel format not supported").

## Consuming it

```bash
tag=ffmpeg-static-v2
curl -fsSL -o ff.tar.xz "https://github.com/TheFunny/ffmpeg-static-win/releases/download/$tag/ffmpeg-static-x64-win.tar.xz"
curl -fsSLO "https://github.com/TheFunny/ffmpeg-static-win/releases/download/$tag/SHA256SUMS.txt"
sha256sum -c SHA256SUMS.txt
mkdir -p ffmpeg-dist && tar xJf ff.tar.xz -C ffmpeg-dist
export FFMPEG_DIR=$PWD/ffmpeg-dist
```

StickerProcess does this in `scripts/fetch-ffmpeg-static.sh`, where the tag and
sha256 are pinned (update them when a new tag is published).

## Local rebuild (fallback)

```powershell
vcpkg install "ffmpeg[avdevice,avformat,avfilter,swscale,swresample,vpx,zlib]" --triplet x64-windows-static
vcpkg install "libvpx[highbitdepth]" --triplet x64-windows-static --recurse
tar cJf ffmpeg-static-x64-win.tar.xz -C <triplet-dir> lib include share
```

Notes kept from the original transition build:

- the vcpkg checkout must be new enough for `avcodec >= 59.37` (FFmpeg 5.1+)
- `avicap32.lib` is not in the Windows SDK (avdevice's vfwcap needs it);
  StickerProcess' `build.rs` and this repo's smoke test both synthesize it from a
  two-symbol `.def`
- debug builds link zlib as `zsd.lib` (release: `zs.lib`)
- verification in the consumer: the app exe must have no ffmpeg DLL imports

Components: FFmpeg LGPLv2.1+, libvpx BSD-3-Clause, zlib license — all fine to
link statically and redistribute.

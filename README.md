# ffmpeg-static-win

x64-windows-static 的 ffmpeg 静态构建产物（`.lib`/headers，vcpkg），供
[StickerProcess](https://github.com/TheFunny/StickerProcess) 桌面 CI 作为
`FFMPEG_DIR` 预构建依赖下载。

## 当前 tag

| tag | 内容 | 来源 |
|---|---|---|
| `ffmpeg-static-v1` | `ffmpeg-static-x64-win.tar.xz`（36 MB，解压后 lib/ include/ share/ 共 ~270 MB） | 本地 vcpkg 构建（过渡阶段） |

解包后目录结构即 vcpkg `installed/x64-windows-static` 子集，直接
`set FFMPEG_DIR=<解包目录>` 可用（StickerProcess 的 `build.rs` 消费
`lib/`；Debug 构建需 debug 库时另见下方配方加 `debug/`）。

## 构建配方（阶段 2 的 CI 脚本种子）

Windows + [vcpkg](https://github.com/microsoft/vcpkg)：

```powershell
vcpkg install "ffmpeg[avdevice,avformat,avfilter,swscale,swresample,vpx,zlib]" --triplet x64-windows-static
vcpkg install "libvpx[highbitdepth]" --triplet x64-windows-static --recurse
```

要点：
- `libvpx[highbitdepth]` 必须有，否则 yuv420p10le 编码报 Invalid argument
- vcpkg checkout 需足够新（ffmpeg ≥ 5.1 / avcodec ≥ 59.37）
- 产物验证：StickerProcess 静态链接后 exe 零 ffmpeg DLL 依赖
- 打包：`tar cJf ffmpeg-static-x64-win.tar.xz -C <triplet-dir> lib include share`

## 计划

本仓库自身加 GitHub Actions（windows runner + vcpkg + binary cache）自动重建并
发布 tag，替换本地过渡件。StickerProcess 侧只需改下载 URL。

组件许可证：FFmpeg LGPLv2.1+、libvpx BSD-3、zlib license——均允许静态链接分发。

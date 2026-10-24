# DtCyber in Docker

Container image for [DtCyber](https://github.com/kej715/DtCyber), the Desktop CYBER
emulator for CDC 6000 / Cyber 170 / Cyber 180 mainframes. The emulated operator
console (DD60) is an **X11** window, so the container draws on your host's X server.

The image is built in three stages:

| Stage     | Purpose                                                                 |
|-----------|-------------------------------------------------------------------------|
| `source`  | Shallow-fetches the sources (plus Git LFS tape images, optional)        |
| `builder` | Compiles `dtcyber` with gcc + X11/Xft headers, runs `npm install` for the helper tools (`make -f Makefile.linux64 all`) |
| `runtime` | Debian slim with only the runtime libs (libX11, libXft, fontconfig, FreeType), Node.js, and tini; runs as non-root user `dtcyber` |

## Requirements (Linux host)

- Docker 20.10+ with BuildKit (the default in current Docker)
- An **X11** session (Xorg). DtCyber upstream does not support Wayland; XWayland may work but is untested.
  Check with `loginctl show-session self -p Type` (or `echo $XDG_SESSION_TYPE`).
- `xhost` (Debian/Ubuntu: `sudo apt install x11-xserver-utils`)
- X core fonts for the default console font (rendered by the **host** X server):
  ```bash
  sudo apt install xfonts-base xfonts-75dpi xfonts-100dpi
  xset fp rehash        # or log out and back in
  ```

## Build

```bash
docker build -t dtcyber .
```

Build arguments:

| Argument       | Default                                   | Description |
|----------------|-------------------------------------------|-------------|
| `DTCYBER_REF`  | `main`                                    | Branch, tag or commit SHA to build |
| `DTCYBER_REPO` | `https://github.com/kej715/DtCyber.git`   | Git repository (e.g. a fork) |
| `FETCH_LFS`    | `1`                                       | `0` skips the ~130 MB of Git LFS tape images |
| `UID` / `GID`  | `1000` / `1000`                           | UID/GID of the in-container `dtcyber` user |

Example, pinned to a commit and matching your host user:

```bash
docker build -t dtcyber \
  --build-arg DTCYBER_REF=<commit-sha> \
  --build-arg UID=$(id -u) --build-arg GID=$(id -g) .
```

> Docker caches the clone layer. To pick up new upstream commits, pass a new
> `DTCYBER_REF` or build with `--no-cache`.

## Run with X11 forwarding (Linux)

1. **Let local containers use your X server** (undo later with `xhost -local:`):
   ```bash
   xhost +local:
   ```
2. **First run: install NOS 2.8.7** using the upstream Node.js automation. The console
   window opens on your desktop. The installed system is kept in the named volume
   `dtcyber-nos287`. On first use Docker copies the kit from the image into that volume.
   ```bash
   docker run -it --rm \
     -e DISPLAY=$DISPLAY \
     -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
     -v dtcyber-nos287:/opt/dtcyber/NOS2.8.7 \
     --network host \
     dtcyber node install
   ```
3. **Later runs: boot the installed system.**
   ```bash
   docker run -it --rm \
     -e DISPLAY=$DISPLAY \
     -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
     -v dtcyber-nos287:/opt/dtcyber/NOS2.8.7 \
     --network host \
     dtcyber node start
   ```

#### How the image runs commands

- The working directory is `/opt/dtcyber/NOS2.8.7`. `tini` is PID 1 and runs whatever command you pass.
- With no command, the container runs `dtcyber`, which uses section `[cyber]` of `./cyber.ini`.
  That only works after an install.
- Raw emulator: `docker run ... dtcyber dtcyber [<ini-section> [<ini-file>]]`, e.g. `dtcyber manual`.
- Other kits: add `-w /opt/dtcyber/NOS1.3` (or `KRONOS2.1`, `HUSTLER`, `COS`) and run
  `node install` there. Use a separate volume for each kit.
- Upstream says to run these with `sudo`. You don't need it here: the `dtcyber` binary
  has `CAP_NET_BIND_SERVICE`, so it can bind ports 21–23 as the non-root user.
- Keep `-it`: DtCyber reads operator commands from stdin and shuts down on EOF.

#### Using your X cookie instead of `xhost`

This is tighter: only containers that have your cookie can connect.

```bash
docker run -it --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
  -v "${XAUTHORITY:-$HOME/.Xauthority}:/home/dtcyber/.Xauthority:ro" \
  -v dtcyber-nos287:/opt/dtcyber/NOS2.8.7 \
  --network host \
  dtcyber node start
```
(Build with `--build-arg UID=$(id -u)` so the cookie file is readable.)

#### Networking

`--network host` is the simplest way to make the emulator's operator port (6662) and
guest services (Telnet/FTP on 21–23, NJE, RJE, etc.) reachable. If you'd rather keep
network isolation, use `-p` mappings instead, e.g. `-p 2323:23 -p 6662:6662`.

## Fonts

- **Default (`lucidatypewriter`)** is an X *core* font, so the host X server draws it.
  Install `xfonts-75dpi`/`xfonts-100dpi` **on the host**.
- **TrueType alternative.** The image registers the bundled **Seymour** font with
  fontconfig, and Xft draws it inside the container. To use it, add this to the
  `[console]` section of your `cyber.ini`:
  ```ini
  fontType=TrueType
  fontName=Seymour
  ```

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `Can't open display` / `Authorization required` | Run `xhost +local:` or mount your `.Xauthority`; check that `DISPLAY` is set on the host |
| `Could not open font ...` or the window has no text | Install the X core fonts on the host (see above) or switch to the TrueType font |
| Window does not appear under Wayland | Log in to an Xorg session (XWayland is unsupported upstream) |
| Permission errors on bind-mounted directories | Rebuild with `--build-arg UID=$(id -u) --build-arg GID=$(id -g)` |
| `bind: permission denied` on ports < 1024 | Don't use `--security-opt no-new-privileges` (it blocks the file capability), or change the ports in `cyber.ini` |

## Notes

- DtCyber is licensed under GPL-3.0. See `/opt/dtcyber/license-gpl-3.0.txt` in the image.
- The commit that was built is recorded in `/opt/dtcyber/.dtcyber-commit`.

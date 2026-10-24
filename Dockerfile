# syntax=docker/dockerfile:1
#
# =============================================================================
#  DtCyber - CDC Cyber mainframe emulator (https://github.com/kej715/DtCyber)
# =============================================================================
#
#  Multi-stage build:
#    1. "source"  - fetches the DtCyber sources (optionally with Git LFS tapes)
#    2. "builder" - compiles the native `dtcyber` binary (C / X11 / Xft) and
#                   installs the Node.js helper tools (`make -f Makefile.linux64 all`)
#    3. "runtime" - slim image with only the runtime shared libraries, Node.js
#                   (needed by the NOS/KRONOS install automation), and fonts.
#
#  BUILD
#  -----
#    docker build -t dtcyber .
#
#    Useful build arguments:
#      --build-arg DTCYBER_REF=<branch|tag|commit>   (default: main)
#      --build-arg DTCYBER_REPO=<git url>            (default: kej715/DtCyber)
#      --build-arg FETCH_LFS=0                       skip the ~130 MB of Git LFS tape images
#      --build-arg UID=$(id -u) --build-arg GID=$(id -g)
#                                                    match your host user (helps with
#                                                    X11 auth & bind-mounted volumes)
#    The clone layer is cached; pass a commit SHA as DTCYBER_REF (or use
#    --no-cache) to pick up new upstream commits.
#
#  RUN (Linux host with an X11 / Xorg session; Wayland-only is NOT supported,
#  XWayland may work but is untested upstream)
#  -----------------------------------------------------------------------------
#    # 1. Allow local containers to talk to your X server (revert: xhost -local:)
#    xhost +local:
#
#    # 2. First run: install NOS 2.8.7 (upstream Node.js automation). The
#    #    console window opens on your desktop; the installed system is kept
#    #    in the named volume "dtcyber-nos287".
#    docker run -it --rm \
#        -e DISPLAY=$DISPLAY \
#        -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
#        -v dtcyber-nos287:/opt/dtcyber/NOS2.8.7 \
#        --network host \
#        dtcyber node install
#
#    # 3. Later runs: boot the installed system
#    docker run -it --rm -e DISPLAY=$DISPLAY \
#        -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
#        -v dtcyber-nos287:/opt/dtcyber/NOS2.8.7 \
#        --network host dtcyber node start
#
#    # Alternative to xhost: share your X cookie (build with UID=$(id -u))
#    #   add: -v $HOME/.Xauthority:/home/dtcyber/.Xauthority:ro
#
#    # Other kits: -w /opt/dtcyber/NOS1.3 (or KRONOS2.1, HUSTLER, COS, ...)
#    # Raw emulator: ... dtcyber dtcyber [<ini-section> [<ini-file>]]
#
#  NOTES
#    * The default console font ("lucidatypewriter") is an X *core* font, which
#      is rendered by the X server on the HOST. Install `xfonts-75dpi` and
#      `xfonts-100dpi` on the host (then `xset fp rehash` or re-login), or set
#      `fontType=TrueType` / `fontName=Seymour` in the [console] section of
#      cyber.ini to use the bundled TrueType font rendered inside the container.
#    * DtCyber's guest services use privileged ports (21/22/23). The binary is
#      granted CAP_NET_BIND_SERVICE so it can bind them as the non-root user.
# =============================================================================

ARG DEBIAN_VERSION=bookworm

# -----------------------------------------------------------------------------
# Stage 1: fetch sources
# -----------------------------------------------------------------------------
FROM debian:${DEBIAN_VERSION}-slim AS source

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates git git-lfs \
 && rm -rf /var/lib/apt/lists/*

ARG DTCYBER_REPO=https://github.com/kej715/DtCyber.git
ARG DTCYBER_REF=main
ARG FETCH_LFS=1

WORKDIR /src
# Shallow fetch of a single ref (works for branches, tags and commit SHAs).
# LFS smudging is deferred so it can be made optional via FETCH_LFS.
RUN git init -q . \
 && git remote add origin "${DTCYBER_REPO}" \
 && GIT_LFS_SKIP_SMUDGE=1 git fetch -q --depth 1 origin "${DTCYBER_REF}" \
 && GIT_LFS_SKIP_SMUDGE=1 git checkout -q FETCH_HEAD \
 && if [ "${FETCH_LFS}" = "1" ]; then git lfs install --local && git lfs pull; fi \
 && git log -1 --format='DtCyber commit: %H (%cd)' | tee /src/.dtcyber-commit \
 && rm -rf .git

# -----------------------------------------------------------------------------
# Stage 2: build
# -----------------------------------------------------------------------------
FROM debian:${DEBIAN_VERSION}-slim AS builder

# Toolchain + X11/Xft/FreeType headers + Node.js/npm for the helper tools.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        libx11-dev \
        libxft-dev \
        libfontconfig1-dev \
        libfreetype6-dev \
        nodejs \
        npm \
        ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY --from=source /src/ ./

# Pick the generic 64-bit Linux makefile on any 64-bit arch (amd64 & arm64);
# the arch-specific ARM makefiles use 32-bit-only compiler flags.
# `all` builds the dtcyber binary and runs `npm install` for automation,
# rje-station, stk, webterm and webterm/www/js.
RUN make -j"$(nproc)" -f Makefile.linux64 dtcyber \
 && make -f Makefile.linux64 all \
 && npm cache clean --force \
 && rm -f ./*.o \
 && test -x ./dtcyber

# -----------------------------------------------------------------------------
# Stage 3: runtime
# -----------------------------------------------------------------------------
FROM debian:${DEBIAN_VERSION}-slim AS runtime

LABEL org.opencontainers.image.title="DtCyber" \
      org.opencontainers.image.description="Desktop CYBER: CDC 6000/Cyber 170/180 mainframe emulator (X11 console)" \
      org.opencontainers.image.source="https://github.com/kej715/DtCyber" \
      org.opencontainers.image.licenses="GPL-3.0-only"

# Runtime libraries only: X11 client + Xft/fontconfig/FreeType, Node.js for
# the install/automation scripts, tini as PID 1 (signal forwarding, reaping
# the processes spawned by the Node.js automation).
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        libx11-6 \
        libxft2 \
        libfontconfig1 \
        libfreetype6 \
        fontconfig \
        fonts-dejavu-core \
        nodejs \
        ca-certificates \
        tini \
        libcap2-bin \
 && rm -rf /var/lib/apt/lists/*

# Unprivileged runtime user (UID/GID overridable to match the host user).
ARG UID=1000
ARG GID=1000
RUN groupadd --gid "${GID}" dtcyber \
 && useradd --uid "${UID}" --gid dtcyber --create-home --shell /bin/bash dtcyber

# Application tree (binary, cyber.ini, OS install kits, Node.js tools).
COPY --from=builder --chown=dtcyber:dtcyber /src/ /opt/dtcyber/

# Register the bundled "Seymour" TrueType console font with fontconfig and
# let the emulator bind privileged guest ports (21/22/23) as non-root.
RUN ! ldd /opt/dtcyber/dtcyber | grep -q "not found" \
 && install -D -m 0644 /opt/dtcyber/fonts/seymour.ttf /usr/local/share/fonts/dtcyber/seymour.ttf \
 && fc-cache -f \
 && setcap cap_net_bind_service=+ep /opt/dtcyber/dtcyber \
 && apt-get purge -y --auto-remove libcap2-bin \
 && ln -s /opt/dtcyber/dtcyber /usr/local/bin/dtcyber

ENV DISPLAY=:0 \
    PATH=/opt/dtcyber:${PATH}

USER dtcyber
# Default to the NOS 2.8.7 kit: its cyber.ini defines the [cyber] section and
# its Node.js automation locates the emulator as ../dtcyber.
WORKDIR /opt/dtcyber/NOS2.8.7

# Operator port (6662), Telnet/FTP (21-23), NJE/trunk ports, etc. are defined
# per configuration in cyber.ini; listed here for documentation purposes.
EXPOSE 21 22 23 6662

# tini is PID 1; the command is whatever you pass, e.g.
#   docker run ... dtcyber node install     # first-time NOS 2.8.7 install
#   docker run ... dtcyber node start       # boot the installed system
#   docker run ... dtcyber dtcyber manual   # raw emulator, [manual] section
# With no command, DtCyber starts section [cyber] of ./cyber.ini (requires a
# completed install).
ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["dtcyber"]

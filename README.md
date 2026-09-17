# YouTube Lite

A small native GTK4/WebKitGTK YouTube client for Linux, designed around older hardware.

## Prototype v0.2

- Native C/GTK4 application
- WebKitGTK 6.0
- Minimal toolbar
- Persistent WebKit website data
- YouTube-focused navigation
- Normal / Lite / Ultra Lite / Automatic modes
- Native web-process request filtering for common advertising/analytics hosts
- YouTube page-element filtering
- Automatic mode adapts between Normal, Lite, and Ultra Lite using available system memory
- Flatpak manifest

## Build on a GNOME/Flatpak development system

Install Flatpak and flatpak-builder, then run:

```bash
flatpak-builder --user --install --force-clean build io.github.aziz.YouTubeLite.yml
flatpak run io.github.aziz.YouTubeLite
```

## Status

This is an early prototype. v0.2 adds a WebKit web-process extension for resource/request filtering and a first Automatic mode. Filtering is intentionally conservative because YouTube changes frequently; this is not a drop-in replacement for uBlock Origin. Automatic mode uses available system memory as a coarse signal and can switch the effective filtering profile. Further versions should add measured CPU/RAM telemetry, more robust rule sets, persistence for preferences, and hardware-decoding diagnostics.

## Flathub

The project should not be submitted until it has a meaningful development history, polished desktop integration, complete metadata/screenshots, reproducible source builds, and passes Flathub validation/linting.

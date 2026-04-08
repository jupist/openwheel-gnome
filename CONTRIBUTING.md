# Contributing to OpenWheel

OpenWheel is a GNOME/Wayland-first dial driver for the ASUS ProArt Dial. Contributions are welcome — bug fixes, new profiles, UX improvements, and KDE Plasma compatibility fixes are all appreciated.

## Workflow

1. Fork the repository on GitHub.
2. Create a topic branch (`git checkout -b fix/my-fix`).
3. Make your changes and run the tests (`cd build && ctest --output-on-failure`).
4. Open a pull request against `main`.

Keep CI green. PRs must pass all checks before merging.

## Stack

| Component | Language | Notes |
|---|---|---|
| `openwheel-daemon` | C11 | No C++. Reads HID, broadcasts D-Bus, exposes uinput. |
| `openwheel-gadget` | C++20 / Qt 6 QML | GNOME-first; KDE Frameworks 6 is fully optional. |
| Profiles | JSON | No code required for new app profiles. |

- **Qt 6** is required. Do not introduce Qt 5-only APIs.
- **KDE Frameworks 6** is optional — the gadget must build and run correctly with `-DNO_KDE_FRAMEWORKS`.
- Do not add runtime dependencies beyond what is listed in the README prerequisites.

## Adding a profile

Create `profiles/<appname>.json` following the format in `profiles/system-default.json`. Set `"enabled": false` if the profile targets a niche application (Blender, Krita style). Test it by placing the file in `~/.local/share/openwheel/profiles/` and restarting the gadget.

## Coding style

- Follow the existing formatting in each file (no style reformatter PRs).
- Prefer small, focused commits with a clear subject line.
- New files must include an SPDX licence header consistent with existing sources.
- Avoid breaking the no-KDE build path — guard KF6 calls with `#ifdef HAVE_KF6`.

## Reporting bugs

Open a GitHub issue and include:
- Fedora/distro version
- GNOME Shell version (`gnome-shell --version`)
- Output of `gdbus call --session --dest org.asus.dial --object-path /org/asus/dial --method org.asus.dial.InjectionAvailable`
- Whether `/dev/hidraw*` and `/dev/uinput` are accessible without root

# Plasma integration

## Target versions

This integration targets KDE Plasma 6 with the Qt 6 and KDE Frameworks 6 (KF6) stacks. The minimum supported versions must remain aligned with:

- Plasma: 6.x
- Qt: 6.x
- KF6: 6.x

Any implementation should assume Wayland-first behavior and avoid reliance on Plasma 5, Qt 5, or KF5 APIs.

## Wayland constraints for overlays

Wayland compositors impose tighter constraints on screen overlays than X11. KWin (the Plasma compositor) controls surface placement, focus, and input routing. As a result:

- Global overlays cannot freely draw above all surfaces without compositor-managed roles.
- Input capture and global shortcuts must be coordinated with KWin or the Plasma Shell, not via direct window manipulation.
- Positioning relative to other surfaces needs compositor-provided protocols or privileged roles (e.g., a KWin effect or Plasma Shell overlay surface).

Designs should avoid assumptions that a standalone application window can behave like a privileged overlay on Wayland.

## Integration points

Consider the following integration paths, each with different capabilities and constraints:

### Plasma applet

A Plasma applet (widget) runs inside Plasma Shell and integrates with panel or desktop containment. It is best for persistent UI, status, or launching controls. Overlays are limited to Plasma Shell-managed surfaces.

### KWin effect

A KWin effect is suitable for compositor-level visuals and input handling. It can render overlays above windows and react to global events, but it requires KWin-specific APIs and ties functionality to the compositor.

### Shell overlay

A Plasma Shell overlay (e.g., a fullscreen or layer-shell style surface managed by Plasma) can present custom UI with higher priority than regular application windows. This approach may require cooperation with Plasma Shell protocols and is still bounded by KWin’s compositor rules.

## DBus API expectations from the daemon

The daemon should expose a stable DBus interface that enables UI components (applet, effect, or overlay) to query state and request actions. Expectations include:

- A well-known DBus service name and object path.
- Read-only properties for current hardware state (e.g., connection status, device info, battery/charge, and telemetry).
- Methods for user-initiated actions (e.g., wake, sleep, recalibrate, or mode switches).
- Signals for state changes to keep UI responsive without polling.
- Versioning or capability discovery to allow UI components to adapt to feature availability.

## Decision gate checklist

Resolve the following before implementing UI:

- [ ] Confirm minimum Plasma 6/Qt 6/KF6 versions to support.
- [ ] Select the primary integration point (Plasma applet vs KWin effect vs Shell overlay).
- [ ] Determine overlay requirements under Wayland (visual priority, input capture, focus behavior).
- [ ] Specify DBus service name, object path, and interface identifiers.
- [ ] Enumerate required DBus properties, methods, and signals.
- [ ] Define capability/version negotiation strategy for the UI.
- [ ] Clarify packaging and deployment constraints for the chosen integration path.

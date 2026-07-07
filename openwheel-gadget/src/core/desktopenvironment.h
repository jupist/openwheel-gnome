// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Tiny static helper for runtime detection of session type / desktop
// environment. Used by ActionExecutor to pick a backend (logind vs
// PowerDevil for brightness, KWin shortcut vs keystroke for zoom, etc.).

#pragma once

#include <QString>

namespace DesktopEnvironment {

// XDG_SESSION_TYPE
bool    isWayland();
bool    isX11();
QString sessionType();

// XDG_CURRENT_DESKTOP — case-insensitive substring match.
bool    isGnome();
bool    isKde();
QString currentDesktop();

} // namespace DesktopEnvironment

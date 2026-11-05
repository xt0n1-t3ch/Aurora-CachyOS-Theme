// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Application-wide setup shared by the app and the screenshot driver: style, palette, font and path context.

#pragma once

#include "core/paths.h"

namespace fe {

/// Gives the application the look every window shares: the Finder style over Breeze, the macOS
/// dark palette, SF Pro, and the app's name and icon.
void configureApplication();

/// How the path bar names the start of a path for the user running the app.
[[nodiscard]] PathContext currentPathContext();

} // namespace fe

// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// What screen readers and other assistive technology see of the path bar: one link per path segment, which they can
// read and press. The address field needs none: it is a line edit whose text is the whole path.

#pragma once

namespace fe {

/// Registers the accessibility of fe::PathBar with Qt. Call once, after the QApplication exists.
void installCrumbAccessibility();

} // namespace fe

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.chrome.browser.tasks.tab_management.GridTabSwitcher;

/** Interface to communicate with the start surface. */
public interface StartSurface extends GridTabSwitcher {
    // TODO(crbug.com/982018): Decouple StartSurface and GridTabSwitcher.
}
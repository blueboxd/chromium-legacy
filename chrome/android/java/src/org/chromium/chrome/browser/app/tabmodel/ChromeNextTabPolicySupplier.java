// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeController;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/**
 * Decides to show a next tab by location if overview is open, or by hierarchy otherwise.
 */
public class ChromeNextTabPolicySupplier implements NextTabPolicySupplier {
    private final OverviewModeController mOverviewModeController;

    public ChromeNextTabPolicySupplier(OverviewModeController overviewModeController) {
        mOverviewModeController = overviewModeController;
    }

    @Override
    public @NextTabPolicy Integer get() {
        if (mOverviewModeController != null && mOverviewModeController.overviewVisible()) {
            return NextTabPolicy.LOCATIONAL;
        } else {
            return NextTabPolicy.HIERARCHICAL;
        }
    }
}

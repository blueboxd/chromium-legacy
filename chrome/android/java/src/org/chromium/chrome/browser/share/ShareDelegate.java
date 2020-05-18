// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareParams;

/**
 * Interface to expose sharing to external classes.
 */
public interface ShareDelegate {
    /**
     * Initiate a share based on the provided ShareParams.
     *
     * @param params The share parameters.
     * @param shareDirectly If this share should be sent directly to the last used share target.
     * @param saveLastUsed If the chosen share target should be saved for future reuse.
     */
    void share(ShareParams params, boolean shareDirectly, boolean saveLastUsed);

    /**
     * Initiate a share for the provided Tab.
     *
     * @param currentTab The Tab to be shared.
     * @param shareDirectly If this share should be sent directly to the last used share target.
     */
    void share(Tab currentTab, boolean shareDirectly);
}

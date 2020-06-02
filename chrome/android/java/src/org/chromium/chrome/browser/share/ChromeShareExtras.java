// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.components.browser_ui.share.ShareParams;

/**
 * A container object for passing share extras not contained in {@link ShareParams} to {@link
 * ShareDelegate}.
 *
 * <p>This class contains extras that are used only by Android Share, and should never be
 * componentized. {@link ShareParams} lives in //components and only contains parameters that are
 * used in more than one part of the Chromium codebase.
 */
public class ChromeShareExtras {
    /**
     * Whether to save the chosen activity for future direct sharing.
     */
    private final boolean mSaveLastUsed;

    /**
     * Whether it should share directly with the activity that was most recently used to share. If
     * false, the share selection will be saved.
     */
    private final boolean mShareDirectly;

    public ChromeShareExtras(boolean saveLastUsed, boolean shareDirectly) {
        mSaveLastUsed = saveLastUsed;
        mShareDirectly = shareDirectly;
    }

    /**
     * @return Whether to save the chosen activity for future direct sharing.
     */
    public boolean saveLastUsed() {
        return mSaveLastUsed;
    }

    /**
     * @return Whether it should share directly with the activity that was most recently used to
     * share.
     */
    public boolean shareDirectly() {
        return mShareDirectly;
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.previews;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;

/**
 * Class for preview-related util methods.
 */
public final class Previews {
    /**
     * @param tab Tab object containing the WebContents.
     * @return {@code true} if the page being displayed is a preview.
     */
    public static boolean isPreview(Tab tab) {
        if (tab == null || tab.isNativePage()) return false;
        WebContents webContents = tab.getWebContents();
        boolean isPreview = webContents != null && !webContents.isShowingInterstitialPage()
                && !SecurityStateModel.isContentDangerous(webContents)
                && PreviewsAndroidBridge.getInstance().shouldShowPreviewUI(webContents);
        return isPreview;
    }
}

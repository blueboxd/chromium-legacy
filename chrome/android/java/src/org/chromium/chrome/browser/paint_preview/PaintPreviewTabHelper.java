// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Manages the Paint Preview component for a given {@link Tab}. Destroyed together with the tab.
 */
public class PaintPreviewTabHelper extends EmptyTabObserver {
    public static void createForTab(Tab tab) {
        new PaintPreviewTabHelper(tab);
    }

    private PaintPreviewTabHelper(Tab tab) {
        tab.addObserver(this);
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        if (qualifiesForCapture(tab)) {
            PaintPreviewExperiments.runCaptureExperiment(tab.getWebContents());
        }
    }

    /**
     * Checks whether a given {@link Tab} qualifies for Paint Preview capture.
     */
    private boolean qualifiesForCapture(Tab tab) {
        return !tab.isIncognito() && !tab.isNativePage() && !tab.isShowingErrorPage()
                && tab.getWebContents() != null;
    }
}

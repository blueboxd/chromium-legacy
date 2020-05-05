// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/**
 * Responsible for checking for and displaying Paint Previews that are associated with a
 * {@link Tab} by overlaying the content view.
 */
public class TabbedPaintPreviewPlayer {
    private Tab mTab;
    private PaintPreviewTabService mPaintPreviewTabService;
    private PlayerManager mPlayerManager;

    /**
     * Shows a Paint Preview for the provided tab if it exists.
     * @param tab The tab to show.
     * @param shownCallback returns true once the Paint Preview is shown or false if showing failed.
     * @return a boolean indicating whether a Paint Preview exists for the tab.
     */
    public boolean showIfExistsForTab(Tab tab, Callback<Boolean> shownCallback) {
        if (mTab != null) removePaintPreview();

        mTab = tab;
        if (mPaintPreviewTabService == null) {
            mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
        }

        // Check if a capture exists. This is a quick check using a cache.
        boolean hasCapture = mPaintPreviewTabService.hasCaptureForTab(tab.getId());
        if (hasCapture) {
            mPlayerManager = new PlayerManager(mTab.getUrl(), mTab.getContext(),
                    mPaintPreviewTabService, String.valueOf(mTab.getId()),
                    TabbedPaintPreviewPlayer.this::onLinkClicked, safeToShow -> {
                        addPlayerView(safeToShow, shownCallback);
                    }, TabThemeColorHelper.getBackgroundColor(mTab));
        }

        return hasCapture;
    }

    /**
     * Removes the view containing the Paint Preview from the most recently shown {@link Tab}. Does
     * nothing if there is no view showing.
     */
    public void removePaintPreview() {
        if (mTab == null || mPlayerManager == null) return;

        mTab.getContentView().removeView(mPlayerManager.getView());
        mPlayerManager.destroy();
        mPlayerManager = null;
        mTab = null;
    }

    private void addPlayerView(boolean safeToShow, Callback<Boolean> shownCallback) {
        if (safeToShow) {
            mTab.getContentView().addView(mPlayerManager.getView());
        } else {
            mTab = null;
            mPlayerManager = null;
        }

        shownCallback.onResult(safeToShow);
    }

    private void onLinkClicked(GURL url) {
        if (mTab == null || !url.isValid() || url.isEmpty()) return;

        mTab.loadUrl(new LoadUrlParams(url.getSpec()));
        removePaintPreview();
    }
}

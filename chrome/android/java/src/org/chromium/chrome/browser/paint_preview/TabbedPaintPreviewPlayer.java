// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tab.TabViewManager;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/**
 * Responsible for checking for and displaying Paint Previews that are associated with a
 * {@link Tab} by overlaying the content view.
 */
public class TabbedPaintPreviewPlayer implements TabViewProvider, UserData {
    public static final Class<TabbedPaintPreviewPlayer> USER_DATA_KEY =
            TabbedPaintPreviewPlayer.class;

    private Tab mTab;
    private PaintPreviewTabService mPaintPreviewTabService;
    private PlayerManager mPlayerManager;
    private Runnable mOnShown;
    private Runnable mOnDismissed;

    public static TabbedPaintPreviewPlayer get(Tab tab) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabbedPaintPreviewPlayer(tab));
        }
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabbedPaintPreviewPlayer(Tab tab) {
        mTab = tab;
        mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
    }

    /**
     * Shows a Paint Preview for the provided tab if it exists.
     * @param onShown The callback for when the Paint Preview is shown.
     * @param onDismissed The callback for when the Paint Preview is dismissed.
     * @return a boolean indicating whether a Paint Preview exists for the tab.
     */
    public boolean showIfExists(@Nullable Runnable onShown, @Nullable Runnable onDismissed) {
        if (isShowing()) return true;

        // Check if a capture exists. This is a quick check using a cache.
        boolean hasCapture = mPaintPreviewTabService.hasCaptureForTab(mTab.getId());
        mOnShown = onShown;
        mOnDismissed = onDismissed;
        if (hasCapture) {
            mPlayerManager = new PlayerManager(mTab.getUrl(), mTab.getContext(),
                    mPaintPreviewTabService, String.valueOf(mTab.getId()), this::onLinkClicked,
                    this::removePaintPreview,
                    () -> TabViewManager.get(mTab).addTabViewProvider(this),
                    TabThemeColorHelper.getBackgroundColor(mTab), this::removePaintPreview);
        }

        return hasCapture;
    }

    /**
     * Removes the view containing the Paint Preview from the most recently shown {@link Tab}. Does
     * nothing if there is no view showing.
     */
    private void removePaintPreview() {
        if (mTab == null || mPlayerManager == null) return;

        TabViewManager.get(mTab).removeTabViewProvider(this);
        mPlayerManager.destroy();
        mPlayerManager = null;
        mOnShown = null;
        mOnDismissed = null;
    }

    public boolean isShowing() {
        return TabViewManager.get(mTab).getCurrentTabViewProvider() == this;
    }

    private void onLinkClicked(GURL url) {
        if (mTab == null || !url.isValid() || url.isEmpty()) return;

        removePaintPreview();
        mTab.loadUrl(new LoadUrlParams(url.getSpec()));
    }

    @Override
    public int getTabViewProviderType() {
        return Type.PAINT_PREVIEW;
    }

    @Override
    public View getView() {
        return mPlayerManager == null ? null : mPlayerManager.getView();
    }

    @Override
    public void onShown() {
        if (mOnShown != null) mOnShown.run();
    }

    @Override
    public void onHidden() {
        if (mOnDismissed != null) mOnDismissed.run();
    }

    @Override
    public void destroy() {
        removePaintPreview();
        mTab = null;
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
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
    private Runnable mOnDismissed;
    private Boolean mInitializing;
    private boolean mHasUserInteraction;
    private EmptyTabObserver mTabObserver;

    public static TabbedPaintPreviewPlayer get(Tab tab) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabbedPaintPreviewPlayer(tab));
        }
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabbedPaintPreviewPlayer(Tab tab) {
        mTab = tab;
        mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                if (mTab.getTabViewManager().isShowing(TabbedPaintPreviewPlayer.this)
                        && !mHasUserInteraction) {
                    removePaintPreview();
                }
            }
        };
        mTab.addObserver(mTabObserver);
    }

    /**
     * Shows a Paint Preview for the provided tab if it exists and has not been displayed for this
     * Tab before.
     * @param onShown The callback for when the Paint Preview is shown.
     * @param onDismissed The callback for when the Paint Preview is dismissed.
     * @return Whether the Paint Preview started to initialize or is already initializating.
     * Note that if the Paint Preview is already showing, this will return false.
     */
    public boolean maybeShow(@Nullable Runnable onShown, @Nullable Runnable onDismissed) {
        if (mInitializing != null) return mInitializing;

        // Check if a capture exists. This is a quick check using a cache.
        boolean hasCapture = mPaintPreviewTabService.hasCaptureForTab(mTab.getId());
        mInitializing = hasCapture;
        if (!hasCapture) return false;

        mPlayerManager = new PlayerManager(mTab.getUrl(), mTab.getContext(),
                mPaintPreviewTabService, String.valueOf(mTab.getId()), this::onLinkClicked,
                this::removePaintPreview,
                () -> {
                    mInitializing = false;
                    onShown.run();
                },
                () -> mHasUserInteraction = true,
                ChromeColors.getPrimaryBackgroundColor(mTab.getContext().getResources(), false),
                this::removePaintPreview, /*ignoreInitialScrollOffset=*/false);
        mOnDismissed = onDismissed;
        mTab.getTabViewManager().addTabViewProvider(this);
        return true;
    }

    /**
     * Removes the view containing the Paint Preview from the most recently shown {@link Tab}. Does
     * nothing if there is no view showing.
     */
    private void removePaintPreview() {
        mOnDismissed = null;
        mInitializing = false;
        if (mTab == null || mPlayerManager == null) return;

        mTab.getTabViewManager().removeTabViewProvider(this);
        mPlayerManager.destroy();
        mPlayerManager = null;
    }

    public boolean isShowing() {
        return mTab.getTabViewManager().isShowing(this);
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
    public void onHidden() {
        if (mOnDismissed != null) mOnDismissed.run();
    }

    @Override
    public void destroy() {
        removePaintPreview();
        mTab.removeObserver(mTabObserver);
        mTab = null;
    }
}

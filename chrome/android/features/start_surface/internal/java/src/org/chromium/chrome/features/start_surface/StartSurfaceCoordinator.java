// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.support.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.GridTabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final GridTabSwitcher mGridTabSwitcher;
    private BottomBarCoordinator mBottomBarCoordinator;
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;

    public StartSurfaceCoordinator(ChromeActivity activity) {
        mGridTabSwitcher =
                TabManagementModuleProvider.getDelegate().createGridTabSwitcher(activity);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TWO_PANES_START_SURFACE_ANDROID)) {
            mBottomBarCoordinator = new BottomBarCoordinator();
            mExploreSurfaceCoordinator = new ExploreSurfaceCoordinator();
        }
    }

    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        mGridTabSwitcher.setOnTabSelectingListener(listener);
    }

    @Override
    public GridController getGridController() {
        return mGridTabSwitcher.getGridController();
    }

    @Override
    public int getResourceId() {
        return mGridTabSwitcher.getResourceId();
    }

    @Override
    public long getLastDirtyTimeForTesting() {
        return mGridTabSwitcher.getLastDirtyTimeForTesting();
    }

    @Override
    public boolean prepareOverview() {
        return mGridTabSwitcher.prepareOverview();
    }

    @Override
    public void postHiding() {
        mGridTabSwitcher.postHiding();
    }

    @Override
    @NonNull
    public Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate) {
        return mGridTabSwitcher.getThumbnailLocationOfCurrentTab(forceUpdate);
    }

    @Override
    @VisibleForTesting
    public void setBitmapCallbackForTesting(Callback<Bitmap> callback) {
        mGridTabSwitcher.setBitmapCallbackForTesting(callback);
    }

    @Override
    @VisibleForTesting
    public int getBitmapFetchCountForTesting() {
        return mGridTabSwitcher.getBitmapFetchCountForTesting();
    }

    @Override
    @VisibleForTesting
    public int getSoftCleanupDelayForTesting() {
        return mGridTabSwitcher.getSoftCleanupDelayForTesting();
    }

    @Override
    @VisibleForTesting
    public int getCleanupDelayForTesting() {
        return mGridTabSwitcher.getCleanupDelayForTesting();
    }
}
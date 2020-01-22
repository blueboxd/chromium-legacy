// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.ComponentName;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Class for handling tab reparenting operations across multiple activities.
 */
public class TabReparentingParams implements AsyncTabParams {
    private static final int TAB_INDEX_NOT_SET = -1;

    private final Tab mTabToReparent;
    private final Intent mOriginalIntent;
    private final Runnable mFinalizeCallback;

    private int mTabIndex = TAB_INDEX_NOT_SET;
    private boolean mIsFromNightModeReparenting;
    private boolean mIsForegroundTab;

    /**
     * Basic constructor for {@link TabReparentingParams}.
     */
    public TabReparentingParams(
            Tab tabToReparent, Intent originalIntent, Runnable finalizeCallback) {
        mTabToReparent = tabToReparent;
        mOriginalIntent = originalIntent;
        mFinalizeCallback = finalizeCallback;
    }

    @Override
    public LoadUrlParams getLoadUrlParams() {
        return null;
    }

    @Override
    public Intent getOriginalIntent() {
        return mOriginalIntent;
    }

    @Override
    public Integer getRequestId() {
        return null;
    }

    @Override
    public WebContents getWebContents() {
        return null;
    }

    @Override
    public ComponentName getComponentName() {
        return null;
    }

    @Override
    public Tab getTabToReparent() {
        return mTabToReparent;
    }

    public boolean hasTabToReparent() {
        return mTabToReparent != null;
    }

    /**
     * Returns the callback to be used once Tab reparenting has finished, if any.
     */
    public @Nullable Runnable getFinalizeCallback() {
        return mFinalizeCallback;
    }

    @Override
    public void destroy() {
        if (mTabToReparent != null) mTabToReparent.destroy();
    }

    // Night mode reparenting implementation.

    /** Set the tab index for later retrieval. */
    public void setTabIndex(int tabIndex) {
        mTabIndex = tabIndex;
    }

    /** @return Index of the stored tab. */
    public int getTabIndex() {
        return mTabIndex;
    }

    /** @return Whether this holds a tab index. */
    public boolean hasTabIndex() {
        return mTabIndex != TAB_INDEX_NOT_SET;
    }

    /** Set whether these params are from night mode reparenting. */
    public void setFromNightModeReparenting(boolean fromNightModeReparenting) {
        mIsFromNightModeReparenting = fromNightModeReparenting;
    }

    /** @return Whether these params are from night mode reparenting. */
    public boolean isFromNightModeReparenting() {
        return mIsFromNightModeReparenting;
    }

    /** Set whether this is the foreground tab. */
    public void setIsForegroundTab(boolean isForegroundTab) {
        mIsForegroundTab = isForegroundTab;
    }

    /** @return Whether this is a foreground tab. */
    public boolean isForegroundTab() {
        return mIsForegroundTab;
    }
}

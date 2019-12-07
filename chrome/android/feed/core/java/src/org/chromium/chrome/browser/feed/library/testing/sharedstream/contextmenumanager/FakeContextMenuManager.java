// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.sharedstream.contextmenumanager;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;

import java.util.List;

/** Fake to be used to test {@link ContextMenuManager}. */
public class FakeContextMenuManager implements ContextMenuManager {
    private boolean mIsMenuOpened;
    @Nullable
    private List<String> mItems;
    @Nullable
    private ContextMenuClickHandler mHandler;

    @Override
    public boolean openContextMenu(
            View anchorView, List<String> items, ContextMenuClickHandler handler) {
        if (mIsMenuOpened) {
            return false;
        }

        mIsMenuOpened = true;
        this.mItems = items;
        this.mHandler = handler;

        return true;
    }

    @Override
    public void dismissPopup() {
        mIsMenuOpened = false;
        mItems = null;
        mHandler = null;
    }

    @Override
    public void setView(View view) {}

    public void performClick(int position) {
        if (!mIsMenuOpened) {
            throw new IllegalStateException("Cannot perform click with no menu opened.");
        }

        checkNotNull(mHandler).handleClick(position);
    }

    public List<String> getMenuOptions() {
        if (!mIsMenuOpened) {
            throw new IllegalStateException("No menu open.");
        }

        return checkNotNull(mItems);
    }
}

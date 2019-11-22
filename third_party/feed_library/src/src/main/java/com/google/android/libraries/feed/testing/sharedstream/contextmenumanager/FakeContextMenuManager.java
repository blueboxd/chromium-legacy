// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.sharedstream.contextmenumanager;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.view.View;

import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;

import java.util.List;

/** Fake to be used to test {@link ContextMenuManager}. */
public class FakeContextMenuManager implements ContextMenuManager {
    private boolean isMenuOpened;
    /*@Nullable*/ private List<String> items;
    /*@Nullable*/ private ContextMenuClickHandler handler;

    @Override
    public boolean openContextMenu(
            View anchorView, List<String> items, ContextMenuClickHandler handler) {
        if (isMenuOpened) {
            return false;
        }

        isMenuOpened = true;
        this.items = items;
        this.handler = handler;

        return true;
    }

    @Override
    public void dismissPopup() {
        isMenuOpened = false;
        items = null;
        handler = null;
    }

    @Override
    public void setView(View view) {}

    public void performClick(int position) {
        if (!isMenuOpened) {
            throw new IllegalStateException("Cannot perform click with no menu opened.");
        }

        checkNotNull(handler).handleClick(position);
    }

    public List<String> getMenuOptions() {
        if (!isMenuOpened) {
            throw new IllegalStateException("No menu open.");
        }

        return checkNotNull(items);
    }
}

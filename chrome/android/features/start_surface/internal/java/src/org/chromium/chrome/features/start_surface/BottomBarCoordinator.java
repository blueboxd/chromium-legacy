// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.BottomBarProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.start_surface.BottomBarProperties.IS_VISIBLE;
import static org.chromium.chrome.features.start_surface.BottomBarProperties.ON_CLICK_LISTENER;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the bottom bar. */
class BottomBarCoordinator {
    private final PropertyModel mBottomBarPropertyModel;
    private final PropertyModelChangeProcessor mBottomBarChangeProcessor;

    BottomBarCoordinator(Context context, ViewGroup parentView, TabModelSelector tabModelSelector) {
        BottomBarView bottomBarView =
                (BottomBarView) LayoutInflater.from(context)
                        .inflate(R.layout.ss_bottom_bar_layout, parentView, true)
                        .findViewById(R.id.ss_bottom_bar);
        mBottomBarPropertyModel = new PropertyModel(BottomBarProperties.ALL_KEYS);
        mBottomBarChangeProcessor = PropertyModelChangeProcessor.create(
                mBottomBarPropertyModel, bottomBarView, BottomBarViewBinder::bind);

        tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mBottomBarPropertyModel.set(IS_INCOGNITO, newModel.isIncognito());
            }
        });

        // Set the initial state.
        mBottomBarPropertyModel.set(IS_INCOGNITO, tabModelSelector.isIncognitoSelected());
    }

    /**
     * Set the visibility of this bottom bar.
     * @param shown Whether sets the visibility to visible or not.
     */
    public void setVisibility(boolean shown) {
        mBottomBarPropertyModel.set(IS_VISIBLE, shown);
    }

    /**
     * Set the {@link BottomBarProperties.OnClickListener}.
     * @param listener Listen clicks.
     */
    public void setOnClickListener(BottomBarProperties.OnClickListener listener) {
        mBottomBarPropertyModel.set(ON_CLICK_LISTENER, listener);
    }
}
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;

import org.chromium.chrome.browser.explore_sites.ExploreSitesCategoryCardView;
import org.chromium.chrome.browser.explore_sites.ExploreSitesSite;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.ui.modelutil.PropertyModel;

class TouchlessExploreSitesCategoryCardView extends ExploreSitesCategoryCardView {

    /**
     * Delegate that's aware of fields necessary for the touchless context menu.
     */
    protected class TouchlessCategoryCardInteractionDelegate extends CategoryCardInteractionDelegate
            implements TouchlessContextMenuManager.Delegate {
        private PropertyModel mModel;

        TouchlessCategoryCardInteractionDelegate(PropertyModel model) {
            super(model.get(ExploreSitesSite.URL_KEY), model.get(ExploreSitesSite.TILE_INDEX_KEY));
            mModel = model;
        }

        @Override
        public String getTitle() {
            return mModel.get(ExploreSitesSite.TITLE_KEY);
        }

        @Override
        public String getContextMenuTitle() {
            return mModel.get(ExploreSitesSite.TITLE_KEY);
        }

        @Override
        public Bitmap getIconBitmap() {
            return mModel.get(ExploreSitesSite.ICON_KEY);
        }

        @Override
        public boolean isItemSupported(int menuItemId) {
            return menuItemId == ContextMenuManager.ContextMenuItemId.REMOVE;
        }
    }

    public TouchlessExploreSitesCategoryCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected CategoryCardInteractionDelegate createInteractionDelegate(PropertyModel model) {
        return new TouchlessCategoryCardInteractionDelegate(model);
    }
}

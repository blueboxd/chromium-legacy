// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the IPH item in {@link TabSwitcherMediator}
 */
class TabGridIphItemCoordinator {
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final TabGridIphItemMediator mMediator;

    TabGridIphItemCoordinator(Context context, TabListRecyclerView contentView, ViewGroup parent) {
        PropertyModel iphItemPropertyModel = new PropertyModel(TabGridIphItemProperties.ALL_KEYS);
        LayoutInflater.from(context).inflate(R.layout.iph_card_item_layout, parent, true);
        TabGridIphItemView iphItemView = parent.findViewById(R.id.tab_grid_iph_item);

        mModelChangeProcessor = PropertyModelChangeProcessor.create(iphItemPropertyModel,
                new TabGridIphItemViewBinder.ViewHolder(contentView, iphItemView),
                TabGridIphItemViewBinder::bind);

        mMediator = new TabGridIphItemMediator(iphItemPropertyModel);
    }

    /**
     * Get the provider to show IPH item.
     * @return The {@link TabSwitcherMediator.IphProvider} used to show IPH.
     */
    TabSwitcherMediator.IphProvider getIphProvider() {
        return mMediator;
    }

    /** Destroy the IPH component. */
    public void destroy() {
        mModelChangeProcessor.destroy();
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.tab_ui.R;

/**
 * {@link TabGridViewHolder} for tab carousel. Owns the tab info card
 * and the associated view hierarchy.
 */
final class TabCarouselViewHolder extends TabGridViewHolder {
    protected TabCarouselViewHolder(View itemView) {
        super(itemView);
    }

    public static TabGridViewHolder create(ViewGroup parent, int itemViewType) {
        final TabGridViewHolder viewHolder = TabGridViewHolder.create(parent, itemViewType);
        // TODO(mattsimmons): Make this more dynamic based on parent/recycler view height.
        final Context context = parent.getContext();
        viewHolder.itemView.getLayoutParams().width =
                context.getResources().getDimensionPixelSize(R.dimen.tab_carousel_card_width);

        return viewHolder;
    }
}

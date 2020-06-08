// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.Resources;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

/**
 * Layout that holds an infobar's contents and provides a background color and a top shadow.
 */
class InfoBarWrapper extends FrameLayout {
    private final InfoBarUiItem mItem;

    /**
     * Constructor for inflating from Java.
     */
    InfoBarWrapper(Context context, InfoBarUiItem item) {
        super(context);
        mItem = item;
        Resources res = context.getResources();
        int peekingHeight = res.getDimensionPixelSize(R.dimen.infobar_peeking_height);
        int shadowHeight = res.getDimensionPixelSize(R.dimen.infobar_shadow_height);
        setMinimumHeight(peekingHeight + shadowHeight);

        // TODO(crbug.com/1025620): Bring in infobar background rather than just setting background
        // to gray. setBackgroundResource() changes the padding, so call setPadding() second.
        // setBackgroundResource(R.drawable.infobar_wrapper_bg);
        setBackgroundColor(0xFFD3D3D3);
        setPadding(0, shadowHeight, 0, 0);
    }

    InfoBarUiItem getItem() {
        return mItem;
    }

    @Override
    public void onViewAdded(View child) {
        child.setLayoutParams(new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.TOP));
    }
}

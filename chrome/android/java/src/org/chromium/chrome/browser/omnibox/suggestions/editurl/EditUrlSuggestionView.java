// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;

/**
 * Container view for Search-Ready Omnibox suggestions.
 * Decorates the suggestion with the bottom bar.
 */
public class EditUrlSuggestionView extends FrameLayout {
    BaseSuggestionView<View> mContent;

    public EditUrlSuggestionView(Context context) {
        super(context, null);
        mContent = new BaseSuggestionView<View>(context, R.layout.omnibox_basic_suggestion);
        LayoutParams contentLayoutParams = generateDefaultLayoutParams();
        contentLayoutParams.width = LayoutParams.MATCH_PARENT;
        contentLayoutParams.height = LayoutParams.WRAP_CONTENT;
        addView(mContent, contentLayoutParams);

        View divider = new View(context, null, 0, R.style.HorizontalDivider);
        LayoutParams dividerLayoutParams = generateDefaultLayoutParams();
        dividerLayoutParams.gravity = Gravity.BOTTOM;
        dividerLayoutParams.width = LayoutParams.MATCH_PARENT;
        dividerLayoutParams.height = getResources().getDimensionPixelSize(R.dimen.divider_height);
        addView(divider, dividerLayoutParams);
    }

    BaseSuggestionView<View> getBaseSuggestionView() {
        return mContent;
    }

    @Override
    public void setSelected(boolean selected) {
        mContent.setSelected(selected);
    }
}

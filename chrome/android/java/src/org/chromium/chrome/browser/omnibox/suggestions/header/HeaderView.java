// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.Context;
import android.text.TextUtils.TruncateAt;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleHorizontalLayoutView;
import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * View for Group Headers.
 */
public class HeaderView extends SimpleHorizontalLayoutView {
    private final TextView mHeaderText;
    private final ImageView mHeaderIcon;

    /**
     * Constructs a new header view.
     *
     * @param context Current context.
     */
    public HeaderView(Context context) {
        super(context);

        TypedValue themeRes = new TypedValue();
        getContext().getTheme().resolveAttribute(R.attr.selectableItemBackground, themeRes, true);
        setBackgroundResource(themeRes.resourceId);
        setClickable(true);
        setFocusable(true);

        mHeaderText = new TextView(context);
        mHeaderText.setLayoutParams(LayoutParams.forDynamicView());
        mHeaderText.setMaxLines(1);
        mHeaderText.setEllipsize(TruncateAt.END);
        mHeaderText.setTextAppearance(ChromeColors.getMediumTextSecondaryStyle(false));
        mHeaderText.setMinHeight(context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_height));
        mHeaderText.setGravity(Gravity.CENTER_VERTICAL);
        mHeaderText.setPaddingRelative(context.getResources().getDimensionPixelSize(
                                               R.dimen.omnibox_suggestion_header_margin_start),
                0, 0, 0);
        addView(mHeaderText);

        mHeaderIcon = new AppCompatImageView(context);
        mHeaderIcon.setScaleType(ImageView.ScaleType.CENTER);
        mHeaderIcon.setImageResource(R.drawable.ic_expand_more_black_24dp);
        mHeaderIcon.setLayoutParams(new LayoutParams(
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_action_icon_width),
                LayoutParams.MATCH_PARENT));
        addView(mHeaderIcon);
    }

    /** Return ImageView used to present group header chevron. */
    ImageView getIconView() {
        return mHeaderIcon;
    }

    /** Return TextView used to present group header text. */
    TextView getTextView() {
        return mHeaderText;
    }
}

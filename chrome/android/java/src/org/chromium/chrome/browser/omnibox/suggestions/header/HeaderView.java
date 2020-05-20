// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils.TruncateAt;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.widget.AppCompatImageView;
import androidx.core.widget.TextViewCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleHorizontalLayoutView;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * View for Group Headers.
 */
public class HeaderView extends SimpleHorizontalLayoutView {
    private final TextView mHeaderText;
    private final ImageView mHeaderIcon;
    private boolean mIsExpanded;
    private Runnable mOnSelectListener;

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
        mHeaderText.setAllCaps(true);
        TextViewCompat.setTextAppearance(
                mHeaderText, ChromeColors.getMediumTextSecondaryStyle(false));
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

    /**
     * Specifies whether view should be announced as expanded or collapsed.
     *
     * @param isExpanded true, if view should be announced as expanded.
     */
    void setExpandedStateForAccessibility(boolean isExpanded) {
        mIsExpanded = isExpanded;
    }

    /**
     * Specify the listener receiving calls when the view is selected.
     */
    void setOnSelectListener(Runnable listener) {
        mOnSelectListener = listener;
    }

    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);
        if (selected && mOnSelectListener != null) {
            mOnSelectListener.run();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            return performClick();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        info.addAction(mIsExpanded ? AccessibilityAction.ACTION_COLLAPSE
                                   : AccessibilityAction.ACTION_EXPAND);
    }

    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        if (action == AccessibilityNodeInfo.ACTION_EXPAND
                || action == AccessibilityNodeInfo.ACTION_COLLAPSE) {
            return performClick();
        }
        return super.performAccessibilityAction(action, arguments);
    }
}

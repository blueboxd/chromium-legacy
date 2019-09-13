// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.util.KeyNavigationUtil;

/**
 * Base layout for common suggestion types. Includes support for a configurable suggestion content
 * and the common suggestion patterns shared across suggestion formats.
 */
public class BaseSuggestionView extends ViewGroup {
    /**
     * Container view for omnibox suggestions allowing soft focus from keyboard.
     */
    private static final class DecoratedSuggestionView extends ViewGroup {
        private View mContentView;

        public DecoratedSuggestionView(Context context) {
            super(context);
        }

        protected void setContentView(View view) {
            if (mContentView != null) removeView(mContentView);
            mContentView = view;
            addView(mContentView);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            mContentView.layout(0, 0, right - left, top - bottom);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;

            mContentView.measure(widthMeasureSpec, heightMeasureSpec);
            setMeasuredDimension(
                    MeasureSpec.getSize(widthMeasureSpec), mContentView.getMeasuredHeight());
        }

        @Override
        public boolean isFocused() {
            return super.isFocused() || (isSelected() && !isInTouchMode());
        }
    }

    protected final ImageView mRefineView;
    protected final DecoratedSuggestionView mContentView;

    private SuggestionViewDelegate mDelegate;
    private boolean mUseDarkIconTint;

    /**
     * Constructs a new suggestion view.
     *
     * @param context The context used to construct the suggestion view.
     */
    public BaseSuggestionView(View view) {
        super(view.getContext());

        TypedValue themeRes = new TypedValue();
        getContext().getTheme().resolveAttribute(R.attr.selectableItemBackground, themeRes, true);
        @DrawableRes
        int selectableBackgroundRes = themeRes.resourceId;

        mContentView = new DecoratedSuggestionView(getContext());
        mContentView.setBackgroundResource(selectableBackgroundRes);
        mContentView.setClickable(true);
        mContentView.setFocusable(true);
        mContentView.setOnClickListener(v -> mDelegate.onSelection());
        mContentView.setOnLongClickListener(v -> {
            mDelegate.onLongPress();
            return true;
        });
        addView(mContentView);

        // Action icons. Currently we only support the Refine button.
        mRefineView = new ImageView(getContext());
        mRefineView.setBackgroundResource(selectableBackgroundRes);
        mRefineView.setClickable(true);
        mRefineView.setFocusable(true);
        mRefineView.setScaleType(ImageView.ScaleType.CENTER);
        mRefineView.setContentDescription(
                getResources().getString(R.string.accessibility_omnibox_btn_refine));
        mRefineView.setImageResource(R.drawable.btn_suggestion_refine);
        mRefineView.setOnClickListener(v -> mDelegate.onRefineSuggestion());

        // Note: height is technically unused, but the behavior is MATCH_PARENT.
        mRefineView.setLayoutParams(new LayoutParams(
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_refine_width),
                LayoutParams.MATCH_PARENT));
        addView(mRefineView);

        Resources res = getResources();

        setContentView(view);
    }

    /**
     * Constructs a new suggestion view and inflates supplied layout as the contents view.
     *
     * @param context The context used to construct the suggestion view.
     * @param layoutId Layout ID to be inflated as the contents view.
     */
    public BaseSuggestionView(Context context, @LayoutRes int layoutId) {
        this(LayoutInflater.from(context).inflate(layoutId, null));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // Note: We layout children in the following order:
        // - first-to-last in LTR orientation and
        // - last-to-first in RTL orientation.
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        int index = isRtl ? getChildCount() - 1 : 0;
        int increment = isRtl ? -1 : 1;

        for (; index >= 0 && index < getChildCount(); index += increment) {
            View v = getChildAt(index);
            if (v.getVisibility() == GONE) continue;
            v.layout(left, 0, left + v.getMeasuredWidth(), bottom - top);
            left += v.getMeasuredWidth();
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        int contentViewWidth = MeasureSpec.getSize(widthSpec);

        // Carve out padding at the end of the object.
        contentViewWidth -= getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);

        // Compute and apply space we can offer to content view.
        for (int index = 0; index < getChildCount(); ++index) {
            View v = getChildAt(index);
            // Do not take mContentView into consideration when computing space for it.
            if (v == mContentView) continue;
            if (v.getVisibility() == GONE) continue;
            LayoutParams p = v.getLayoutParams();
            if (p.width > 0) contentViewWidth -= p.width;
        }

        // Measure height of the content view given the width constraint.
        mContentView.measure(MeasureSpec.makeMeasureSpec(contentViewWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int heightPx = mContentView.getMeasuredHeight();
        heightSpec = MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY);

        // Apply measured dimensions to all children.
        for (int index = 0; index < getChildCount(); ++index) {
            View v = getChildAt(index);

            // Avoid calling (expensive) measure on content view twice.
            if (v == mContentView) continue;

            v.measure(MeasureSpec.makeMeasureSpec(v.getLayoutParams().width, MeasureSpec.EXACTLY),
                    heightSpec);
        }

        setMeasuredDimension(MeasureSpec.getSize(widthSpec), MeasureSpec.getSize(heightSpec));
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        // Whenever the suggestion dropdown is touched, we dispatch onGestureDown which is
        // used to let autocomplete controller know that it should stop updating suggestions.
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mDelegate.onGestureDown();
        } else if (ev.getActionMasked() == MotionEvent.ACTION_UP) {
            mDelegate.onGestureUp(ev.getEventTime());
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            mDelegate.onRefineSuggestion();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void setSelected(boolean selected) {
        mContentView.setSelected(selected);
        mDelegate.onSetUrlToSuggestion();
    }

    /**
     * Set the content view to supplied view.
     *
     * @param view View to be displayed as suggestion content.
     */
    public void setContentView(View view) {
        mContentView.setContentView(view);
    }

    /** Sets the delegate for the actions on the suggestion view. */
    void setDelegate(SuggestionViewDelegate delegate) {
        mDelegate = delegate;
    }

    /** Change refine button visibility. */
    void setRefineVisible(boolean visible) {
        mRefineView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Update the refine icon tint color. */
    void updateIconTint(@ColorInt int color) {
        Drawable drawable = mRefineView.getDrawable();
        DrawableCompat.setTint(drawable, color);
    }
}

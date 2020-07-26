// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.start_surface.R;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;

/**
 * A {@link LinearLayout} that shows loading placeholder for Feed cards with thumbnail on the right.
 */
public class FeedLoadingLayout extends LinearLayout {
    private static final int CARD_NUM = 4;
    private static final int CARD_HEIGHT_DP = 180;
    private static final int CARD_HEIGHT_DENSE_DP = 156;
    private static final int CARD_MARGIN_DP = 12;
    private static final int PLACEHOLDER_CARD_PADDING_DP = 15;
    private static final int IMAGE_PLACEHOLDER_BOTTOM_PADDING_DP = 48;
    private static final int IMAGE_PLACEHOLDER_BOTTOM_PADDING_DENSE_DP = 72;
    private static final int IMAGE_PLACEHOLDER_WIDTH_DP = 107;
    private static final int TEXT_PLACEHOLDER_HEIGHT_DP = 25;
    private static final int TEXT_PLACEHOLDER_RADIUS_DP = 12;

    private final Context mContext;
    private final Resources mResources;
    private long mLayoutInflationCompleteMs;
    private int mScreenWidthDp;
    private int mPaddingPx;

    public FeedLoadingLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mResources = mContext.getResources();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setHeader();
        setPlaceholders();
        mLayoutInflationCompleteMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        setPlaceholders();
    }

    /**
     * Set the header blank for the placeholder.The header blank should be consistent with the
     * sectionHeaderView of {@link ExploreSurfaceCoordinator.FeedSurfaceCreator#}
     */
    @SuppressLint("InflateParams")
    private void setHeader() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        View header;
        // This flag is checked directly with ChromeFeatureList#isEnabled() in other places. Using
        // CachedFeatureFlags#isEnabled here is deliberate for a pre-native check. This
        // inconsistency is fine because the check here is for the Feed header blank size, the
        // mismatch is bearable and only once for every change.
        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)) {
            header = inflater.inflate(
                    R.layout.new_tab_page_snippets_expandable_header_with_menu, null, false);
            header.findViewById(R.id.header_menu).setVisibility(INVISIBLE);
        } else {
            header = inflater.inflate(R.layout.ss_feed_header, null, false);
        }
        LinearLayout headerView = findViewById(R.id.feed_placeholder_header);
        headerView.addView(header);
    }

    private void setPlaceholders() {
        setPadding();
        boolean isLandscape = getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE;
        // If it's in landscape mode, the placeholder should always show in dense mode. Otherwise,
        // whether the placeholder is dense depends on whether the first article card of Feed is
        // dense.
        setPlaceholders(isLandscape || StartSurfaceConfiguration.isFeedPlaceholderDense());
    }

    private void setPlaceholders(boolean isDense) {
        LinearLayout cardsParentView = findViewById(R.id.placeholders_layout);
        cardsParentView.removeAllViews();
        int cardHeight = isDense ? dpToPx(CARD_HEIGHT_DENSE_DP) : dpToPx(CARD_HEIGHT_DP);
        LinearLayout.LayoutParams cardLp =
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, cardHeight);
        cardLp.setMargins(0, 0, 0, dpToPx(CARD_MARGIN_DP));
        LinearLayout.LayoutParams textPlaceholderLp =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1);
        LinearLayout.LayoutParams imagePlaceholderLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        for (int i = 0; i < CARD_NUM; i++) {
            // The card container.
            LinearLayout container = new LinearLayout(mContext);
            container.setLayoutParams(cardLp);
            container.setBackgroundResource(R.drawable.hairline_border_card_background_light);
            container.setOrientation(HORIZONTAL);

            // The placeholder of suggestion titles, context and publisher.
            ImageView textPlaceholder = new ImageView(mContext);
            textPlaceholder.setImageDrawable(setTextPlaceholder(cardHeight));
            textPlaceholder.setLayoutParams(textPlaceholderLp);
            textPlaceholder.setScaleType(ImageView.ScaleType.FIT_XY);
            container.addView(textPlaceholder);

            // The placeholder of image and menu icon.
            ImageView imagePlaceholder = new ImageView(mContext);
            imagePlaceholder.setImageDrawable(setImagePlaceholder(isDense));
            imagePlaceholder.setLayoutParams(imagePlaceholderLp);
            container.addView(imagePlaceholder);

            cardsParentView.addView(container);
        }
    }

    private LayerDrawable setImagePlaceholder(boolean isDense) {
        LayerDrawable layerDrawable = (LayerDrawable) getResources().getDrawable(
                R.drawable.feed_loading_image_placeholder);
        int padding = dpToPx(PLACEHOLDER_CARD_PADDING_DP);
        layerDrawable.setLayerInset(0, 0, padding, padding,
                isDense ? dpToPx(IMAGE_PLACEHOLDER_BOTTOM_PADDING_DP)
                        : dpToPx(IMAGE_PLACEHOLDER_BOTTOM_PADDING_DENSE_DP));
        return layerDrawable;
    }

    private LayerDrawable setTextPlaceholder(int cardHeight) {
        int top = dpToPx(PLACEHOLDER_CARD_PADDING_DP);
        int left = top / 2;
        int right = top;
        int height = dpToPx(TEXT_PLACEHOLDER_HEIGHT_DP);
        int width = dpToPx(mScreenWidthDp) - mPaddingPx * 2 - dpToPx(IMAGE_PLACEHOLDER_WIDTH_DP);
        GradientDrawable[] placeholders = new GradientDrawable[3];
        for (int i = 0; i < placeholders.length; i++) {
            placeholders[i] = new GradientDrawable();
            placeholders[i].setShape(GradientDrawable.RECTANGLE);
            placeholders[i].setSize(width, height);
            placeholders[i].setCornerRadius(dpToPx(TEXT_PLACEHOLDER_RADIUS_DP));
            placeholders[i].setColor(mResources.getColor(R.color.feed_placeholder_color));
        }
        LayerDrawable layerDrawable = new LayerDrawable(placeholders);
        // Title Placeholder
        layerDrawable.setLayerInset(0, left, top, right, cardHeight - top - height);
        // Content Placeholder
        layerDrawable.setLayerInset(
                1, left, (cardHeight - height) / 2, right, (cardHeight - height) / 2);
        // Publisher Placeholder
        layerDrawable.setLayerInset(2, left, cardHeight - top - height, right * 7, top);
        return layerDrawable;
    }

    private int dpToPx(int dp) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, dp, getResources().getDisplayMetrics());
    }

    /**
     * Make the padding of placeholder consistent with that of native articles recyclerview which
     * is resized by {@link ViewResizer} in {@link FeedLoadingCoordinator}
     */
    private void setPadding() {
        int defaultPadding =
                mResources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        UiConfig uiConfig = new UiConfig(this);
        // mUiConfig.getContext().getResources() is used here instead of mView.getResources()
        // because lemon compression, somehow, causes the resources to return a different
        // configuration.
        Resources resources = uiConfig.getContext().getResources();
        mScreenWidthDp = resources.getConfiguration().screenWidthDp;

        if (uiConfig.getCurrentDisplayStyle().horizontal == HorizontalDisplayStyle.WIDE) {
            mPaddingPx = computePadding();
        } else {
            mPaddingPx = defaultPadding;
        }
        setPaddingRelative(mPaddingPx, 0, mPaddingPx, 0);
    }

    private int computePadding() {
        int widePadding = mResources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
        int padding =
                dpToPx((int) ((mScreenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f));
        padding = Math.max(widePadding, padding);

        return padding;
    }

    long getLayoutInflationCompleteMs() {
        return mLayoutInflationCompleteMs;
    }
}

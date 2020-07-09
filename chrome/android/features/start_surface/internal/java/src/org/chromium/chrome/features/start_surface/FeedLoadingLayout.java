// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
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
    private static final int TEXT_PLACEHOLDER_WIDTH_DP = 150;
    private static final int TEXT_PLACEHOLDER_WIDTH_LANDSCAPE_DP = 300;
    private static final int TEXT_PLACEHOLDER_HEIGHT_DP = 25;
    private static final int TEXT_PLACEHOLDER_RADIUS_DP = 11;

    private Context mContext;
    private @Nullable PersonalizedSigninPromoView mSigninPromoView;
    private Resources mResources;
    private long mLayoutInflationCompleteMs;

    public FeedLoadingLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mResources = mContext.getResources();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // TODO (crbug.com/1079443): Inflate article suggestions section header here.
        setPlaceholders();
        mLayoutInflationCompleteMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        setPlaceholders();
    }

    /** @return The {@link PersonalizedSigninPromoView} for this class. */
    PersonalizedSigninPromoView getSigninPromoView() {
        if (mSigninPromoView == null) {
            mSigninPromoView = (PersonalizedSigninPromoView) LayoutInflater.from(mContext).inflate(
                    R.layout.personalized_signin_promo_view_modern_content_suggestions, null,
                    false);
            LinearLayout signView = findViewById(R.id.sign_in_box);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            signView.setLayoutParams(lp);
            lp.setMargins(0, 0, 0, dpToPx(12));
            signView.addView(mSigninPromoView);
        }
        return mSigninPromoView;
    }

    private void setPlaceholders() {
        setPadding();
        boolean isLandscape = getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE;
        LinearLayout cardsParentView = (LinearLayout) findViewById(R.id.placeholders_layout);
        // If it's in landscape mode, the placeholder should always show in dense mode. Otherwise,
        // whether the placeholder is dense depends on whether the first article card of Feed is
        // dense.
        setPlaceholders(cardsParentView,
                isLandscape || StartSurfaceConfiguration.isFeedPlaceholderDense(), isLandscape);
    }

    private void setPlaceholders(
            LinearLayout cardsParentView, boolean isDense, boolean isLandscape) {
        cardsParentView.removeAllViews();
        LinearLayout.LayoutParams cardLp =
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                        isDense ? dpToPx(CARD_HEIGHT_DENSE_DP) : dpToPx(CARD_HEIGHT_DP));
        cardLp.setMargins(0, 0, 0, dpToPx(CARD_MARGIN_DP));
        LinearLayout.LayoutParams textPlaceholderLp =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1);
        LinearLayout.LayoutParams imagePlaceholderLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        for (int i = 0; i < CARD_NUM; i++) {
            // The card container.
            LinearLayout container = new LinearLayout(mContext);
            container.setLayoutParams(cardLp);
            container.setBackgroundResource(R.drawable.hairline_border_card_background);
            container.setOrientation(HORIZONTAL);

            // The placeholder of suggestion titles, context and publisher.
            ImageView textPlaceholder = new ImageView(mContext);
            textPlaceholder.setImageDrawable(setTextPlaceholder(isDense, isLandscape));
            textPlaceholder.setLayoutParams(textPlaceholderLp);
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

    private LayerDrawable setTextPlaceholder(boolean isDense, boolean isLandscape) {
        int cardHeight = isDense ? dpToPx(CARD_HEIGHT_DENSE_DP) : dpToPx(CARD_HEIGHT_DP);
        int top = dpToPx(PLACEHOLDER_CARD_PADDING_DP);
        int bottom = top;
        int left = top;
        int right = top;
        int width = isLandscape ? dpToPx(TEXT_PLACEHOLDER_WIDTH_LANDSCAPE_DP)
                                : dpToPx(TEXT_PLACEHOLDER_WIDTH_DP);
        int height = dpToPx(TEXT_PLACEHOLDER_HEIGHT_DP);
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
        layerDrawable.setLayerInset(1, left, (cardHeight + top - bottom - height) / 2, right,
                (cardHeight - top + bottom - height) / 2);
        // Publisher Placeholder
        layerDrawable.setLayerInset(2, left, cardHeight - bottom - height, right * 10, bottom);
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
        int padding;
        int defaultPadding =
                mResources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        UiConfig uiConfig = new UiConfig(this);

        if (uiConfig.getCurrentDisplayStyle().horizontal == HorizontalDisplayStyle.WIDE) {
            padding = computePadding(uiConfig);
        } else {
            padding = defaultPadding;
        }
        setPaddingRelative(padding, 0, padding, 0);
    }

    private int computePadding(UiConfig uiConfig) {
        // mUiConfig.getContext().getResources() is used here instead of mView.getResources()
        // because lemon compression, somehow, causes the resources to return a different
        // configuration.
        int widePadding = mResources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
        Resources resources = uiConfig.getContext().getResources();
        int screenWidthDp = resources.getConfiguration().screenWidthDp;
        int padding =
                dpToPx((int) ((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f));
        padding = Math.max(widePadding, padding);

        return padding;
    }

    long getLayoutInflationCompleteMs() {
        return mLayoutInflationCompleteMs;
    }
}

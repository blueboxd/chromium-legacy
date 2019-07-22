// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.design.widget.TabLayout;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.widget.ChromeImageView;

/** The bottom bar view. */
class BottomBarView extends FrameLayout {
    private final ColorStateList mNormalButtonTintColor;
    private final ColorStateList mNormalIndicatorColor;
    private final ColorStateList mIncognitoButtonTintColor;
    private final ColorStateList mIncognitoIndicatorColor;

    private TabLayout mTabLayout;
    private TabLayout.Tab mHomeTab;
    private TabLayout.Tab mExploreTab;
    private ChromeImageView mHomeButton;
    private TextView mHomeButtonLabel;
    private ChromeImageView mExploreButton;
    private TextView mExploreButtonLabel;
    private BottomBarProperties.OnClickListener mOnClickListener;

    public BottomBarView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mNormalIndicatorColor =
                AppCompatResources.getColorStateList(context, R.color.light_active_color);
        mNormalButtonTintColor =
                AppCompatResources.getColorStateList(context, R.color.ss_normal_bottom_button_tint);
        mIncognitoButtonTintColor =
                AppCompatResources.getColorStateList(context, R.color.white_alpha_70);
        mIncognitoIndicatorColor =
                AppCompatResources.getColorStateList(context, R.color.white_mode_tint);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTabLayout = (TabLayout) findViewById(R.id.bottom_tab_layout);
        mHomeTab = mTabLayout.getTabAt(0);
        mExploreTab = mTabLayout.getTabAt(1);
        mHomeButton = (ChromeImageView) mTabLayout.findViewById(R.id.ss_home_button);
        mHomeButtonLabel = (TextView) mTabLayout.findViewById(R.id.ss_home_button_label);
        mExploreButton = (ChromeImageView) mTabLayout.findViewById(R.id.ss_explore_button);
        mExploreButtonLabel = (TextView) mTabLayout.findViewById(R.id.ss_explore_button_label);

        mTabLayout.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                if (mOnClickListener == null) return;
                if (tab == mHomeTab) {
                    mOnClickListener.onHomeButtonClicked();
                    return;
                }
                if (tab == mExploreTab) {
                    mOnClickListener.onExploreButtonClicked();
                    return;
                }
                assert false : "Unsupported Tab.";
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}

            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });
    }

    /**
     * Set the incognito state.
     * @param isIncognito Whether is setting the incognito mode.
     */
    public void setIncognito(boolean isIncognito) {
        // TODO(crbug.com/982018): Support dark mode.
        setBackgroundColor(
                ColorUtils.getPrimaryBackgroundColor(getContext().getResources(), isIncognito));
        if (isIncognito) {
            // TODO(crbug.com/982018): Distinguish selected and unselected state in incognito mode.
            mTabLayout.setSelectedTabIndicatorColor(mIncognitoIndicatorColor.getDefaultColor());
            ApiCompatibilityUtils.setImageTintList(mHomeButton, mIncognitoButtonTintColor);
            mHomeButtonLabel.setTextColor(mIncognitoButtonTintColor);
            ApiCompatibilityUtils.setImageTintList(mExploreButton, mIncognitoButtonTintColor);
            mExploreButtonLabel.setTextColor(mIncognitoButtonTintColor);
        } else {
            mTabLayout.setSelectedTabIndicatorColor(mNormalIndicatorColor.getDefaultColor());
            ApiCompatibilityUtils.setImageTintList(mHomeButton, mNormalButtonTintColor);
            mHomeButtonLabel.setTextColor(mNormalButtonTintColor);
            ApiCompatibilityUtils.setImageTintList(mExploreButton, mNormalButtonTintColor);
            mExploreButtonLabel.setTextColor(mNormalButtonTintColor);
        }
    }

    /**
     * Set the visibility of this bottom bar.
     * @param shown Whether set the visibility to visible or not.
     */
    public void setVisibility(boolean shown) {
        setVisibility(shown ? View.VISIBLE : View.INVISIBLE);
    }

    /**
     * Set the {@link BottomBarProperties.OnClickListener}.
     * @param listener Listen clicks.
     */
    public void setOnClickListener(BottomBarProperties.OnClickListener listener) {
        mOnClickListener = listener;
    }
}
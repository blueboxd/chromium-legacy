// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RelativeLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** View of the StartSurfaceToolbar */
class StartSurfaceToolbarView extends RelativeLayout {
    private NewTabButton mNewTabButton;
    private View mIncognitoSwitch;
    private MenuButton mMenuButton;
    private View mLogo;
    private int mPrimaryColor;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;

    public StartSurfaceToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mNewTabButton = findViewById(R.id.new_tab_button);
        mIncognitoSwitch = findViewById(R.id.incognito_switch);
        mMenuButton = findViewById(R.id.menu_button_wrapper);
        mLogo = findViewById(R.id.logo);
        updatePrimaryColorAndTint(false);
    }

    /**
     * @param appMenuButtonHelper The {@link AppMenuButtonHelper} for managing menu button
     *         interactions.
     */
    void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mMenuButton.getImageButton().setOnTouchListener(appMenuButtonHelper);
        mMenuButton.getImageButton().setAccessibilityDelegate(
                appMenuButtonHelper.getAccessibilityDelegate());
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabButton.setOnClickListener(listener);
    }

    /**
     * @param isVisible Whether the Logo is visible.
     */
    void setLogoVisibility(boolean isVisible) {
        mLogo.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @param isVisible Whether the menu button is visible.
     */
    void setMenuButtonVisibility(boolean isVisible) {
        mMenuButton.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /** Called when incognito mode changes. */
    void updateIncognito(boolean isIncognito) {
        updatePrimaryColorAndTint(isIncognito);
    }

    /**
     * @param provider The {@link IncognitoStateProvider} passed to buttons that need access to it.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mNewTabButton.setIncognitoStateProvider(provider);
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mNewTabButton.onAccessibilityStatusChanged();
    }

    private void updatePrimaryColorAndTint(boolean isIncognito) {
        int primaryColor = ChromeColors.getPrimaryBackgroundColor(getResources(), isIncognito);
        setBackgroundColor(primaryColor);

        if (mLightIconTint == null) {
            mLightIconTint =
                    AppCompatResources.getColorStateList(getContext(), R.color.tint_on_dark_bg);
            mDarkIconTint =
                    AppCompatResources.getColorStateList(getContext(), R.color.standard_mode_tint);
        }

        boolean useLightIcons = ColorUtils.shouldUseLightForegroundOnBackground(primaryColor);
        ColorStateList tintList = useLightIcons ? mLightIconTint : mDarkIconTint;
        ApiCompatibilityUtils.setImageTintList(mMenuButton.getImageButton(), tintList);
    }
}

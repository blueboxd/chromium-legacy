// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IPHContainer;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** View of the StartSurfaceToolbar */
class StartSurfaceToolbarView extends RelativeLayout {
    private NewTabButton mNewTabButton;
    private View mIncognitoSwitch;
    private MenuButton mMenuButton;
    private View mLogo;
    @Nullable
    private ImageButton mIdentityDiscButton;
    private int mPrimaryColor;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;

    private Rect mLogoRect = new Rect();
    private Rect mViewRect = new Rect();

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
        mIdentityDiscButton = findViewById(R.id.experimental_toolbar_button_start);

        // Change padding in layout file programmatically, since padding in layout file can not be
        // changed in ViewStub.
        final int buttonPadding = getContext().getResources().getDimensionPixelOffset(
                R.dimen.start_surface_toolbar_button_padding_to_button);
        mIdentityDiscButton.setPadding(buttonPadding, 0, buttonPadding, 0);
        updatePrimaryColorAndTint(false);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // TODO(https://crbug.com/1040526)

        super.onLayout(changed, l, t, r, b);

        if (mLogo.getVisibility() == View.GONE) return;

        mLogoRect.set(mLogo.getLeft(), mLogo.getTop(), mLogo.getRight(), mLogo.getBottom());
        for (int viewIndex = 0; viewIndex < getChildCount(); viewIndex++) {
            View view = getChildAt(viewIndex);
            if (view == mLogo) continue;
            mViewRect.set(view.getLeft(), view.getTop(), view.getRight(), view.getBottom());
            if (Rect.intersects(mLogoRect, mViewRect)) {
                mLogo.setVisibility(View.GONE);
                break;
            }
        }
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
     * Sets the Logo visibility. Logo will not show if screen is not wide enough.
     * @param isVisible Whether the Logo should be visible.
     */
    void setLogoVisibility(boolean isVisible) {
        mLogo.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @param isVisible Whether the menu button is visible.
     */
    void setMenuButtonVisibility(boolean isVisible) {
        mMenuButton.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        final int buttonPaddingLeft = getContext().getResources().getDimensionPixelOffset(
                R.dimen.start_surface_toolbar_button_padding_to_button);
        final int buttonPaddingRight =
                (isVisible ? buttonPaddingLeft
                           : getContext().getResources().getDimensionPixelOffset(
                                   R.dimen.start_surface_toolbar_button_padding_to_edge));
        mIdentityDiscButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
        mNewTabButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
    }

    /**
     * @param isVisible Whether the new tab button is visible.
     */
    void setNewTabButtonVisibility(boolean isVisible) {
        mNewTabButton.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @param isClickable Whether the buttons are clickable.
     */
    void setButtonClickableState(boolean isClickable) {
        mNewTabButton.setClickable(isClickable);
        mIncognitoSwitch.setClickable(isClickable);
        mMenuButton.setClickable(isClickable);
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

    /**
     * @param isVisible Whether the identity disc is visible.
     */
    void setIdentityDiscVisibility(boolean isVisible) {
        mIdentityDiscButton.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the identity disc button is
     * pressed.
     * @param listener The callback that will be notified when the identity disc  is pressed.
     */
    void setIdentityDiscClickHandler(View.OnClickListener listener) {
        mIdentityDiscButton.setOnClickListener(listener);
    }

    /**
     * Updates the image displayed on the identity disc button.
     * @param image The new image for the button.
     */
    void setIdentityDiscImage(Drawable image) {
        mIdentityDiscButton.setImageDrawable(image);
    }

    /**
     * Updates idnetity disc content description.
     * @param contentDescriptionResId The new description for the button.
     */
    void setIdentityDiscContentDescription(@StringRes int contentDescriptionResId) {
        mIdentityDiscButton.setContentDescription(
                getContext().getResources().getString(contentDescriptionResId));
    }

    /**
     * Show the IPH for the identity disc button.
     */
    void showIPHOnIdentityDisc(IPHContainer iphContainer) {
        TextBubble textBubble = new TextBubble(getContext(), mIdentityDiscButton,
                iphContainer.stringId, iphContainer.accessibilityStringId, mIdentityDiscButton);
        textBubble.setDismissOnTouchInteraction(true);
        if (iphContainer.dismissedCallback != null) {
            textBubble.addOnDismissListener(() -> { iphContainer.dismissedCallback.run(); });
        }
        textBubble.show();
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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The controller for the StartSurfaceToolbar. This class handles all interactions that the
 * StartSurfaceToolbar has with the outside world. Lazily creates the tab toolbar the first time
 * it's needed.
 */
class StartSurfaceToolbarCoordinator {
    private final StartSurfaceToolbarMediator mToolbarMediator;
    private final ViewStub mStub;
    private final PropertyModel mPropertyModel;
    private StartSurfaceToolbarView mView;
    private TabModelSelector mTabModelSelector;
    private IncognitoSwitchCoordinator mIncognitoSwitchCoordinator;

    StartSurfaceToolbarCoordinator(ViewStub startSurfaceToolbarStub) {
        mStub = startSurfaceToolbarStub;
        mPropertyModel = new PropertyModel(StartSurfaceToolbarProperties.ALL_KEYS);
        mToolbarMediator = new StartSurfaceToolbarMediator(mPropertyModel);
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        mToolbarMediator.destroy();
    }
    /**
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mToolbarMediator.setAppMenuButtonHelper(appMenuButtonHelper);
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mToolbarMediator.setOnNewTabClickHandler(listener);
    }

    /**
     * Sets the current {@Link TabModelSelector} so the toolbar can pass it into buttons that need
     * access to it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        mToolbarMediator.setTabModelSelector(selector);
    }
    /**
     * Called when Start Surface mode is entered or exited.
     * @param inStartSurfaceMode Whether or not start surface mode should be shown or hidden.
     */
    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        if (!isInflated()) {
            inflate();
        }
        mToolbarMediator.setStartSurfaceMode(inStartSurfaceMode);
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mToolbarMediator.setIncognitoStateProvider(provider);
    }

    /**
     * Called to set the visibility in Start Surface mode.
     * @param shouldShowStartSurfaceToolbar whether the toolbar should be visible.
     */
    void setStartSurfaceToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mToolbarMediator.setStartSurfaceToolbarVisibility(shouldShowStartSurfaceToolbar);
    }

    /**
     * Called when accessibility status changes.
     * @param enabled whether accessibility status is enabled.
     */
    void onAccessibilityStatusChanged(boolean enabled) {
        mToolbarMediator.onAccessibilityStatusChanged(enabled);
    }

    /**
     * @param isVisible Whether the bottom toolbar is visible.
     */
    void onBottomToolbarVisibilityChanged(boolean isVisible) {
        mToolbarMediator.onBottomToolbarVisibilityChanged(isVisible);
    }

    /**
     * @param overviewModeBehavior The {@link OverviewModeBehavior} to observe overview state
     *         changes.
     */
    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mToolbarMediator.setOverviewModeBehavior(overviewModeBehavior);
    }

    /**
     * Enable the experimental toolbar button.
     * @param onClickListener The {@link OnClickListener} to be called when the button is clicked.
     * @param image The drawable to display for the button.
     * @param contentDescriptionResId The resource id of the content description for the button.
     */
    void enableExperimentalButton(View.OnClickListener onClickListener, Drawable image,
            @StringRes int contentDescriptionResId) {
        mToolbarMediator.enableIdentityDisc(onClickListener, image, contentDescriptionResId);
    }

    /**
     * Updates image displayed on experimental button.
     * @param image The new image for the button.
     */
    void updateExperimentalButtonImage(Drawable image) {
        mToolbarMediator.updateIdentityDiscImage(image);
    }

    /**
     * Disable the experimental toolbar button.
     */
    void disableExperimentalButton() {
        mToolbarMediator.disableIdentityDisc();
    }

    /**
     * Displays in-product help for experimental button.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param dismissedCallback The callback that will be called when in-product help is dismissed.
     */
    void showIPHOnExperimentalButton(@StringRes int stringId, @StringRes int accessibilityStringId,
            Runnable dismissedCallback) {
        mToolbarMediator.showIPHOnIdentityDisc(stringId, accessibilityStringId, dismissedCallback);
    }

    void onNativeLibraryReady() {
        mToolbarMediator.onNativeLibraryReady();
    }

    private void inflate() {
        mStub.setLayoutResource(R.layout.start_top_toolbar);
        mView = (StartSurfaceToolbarView) mStub.inflate();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mView, StartSurfaceToolbarViewBinder::bind);

        if (IncognitoUtils.isIncognitoModeEnabled()) {
            mIncognitoSwitchCoordinator = new IncognitoSwitchCoordinator(mView, mTabModelSelector);
        }
    }

    private boolean isInflated() {
        return mView != null;
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.support.v7.content.res.AppCompatResources;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;

/**
 * Handles displaying share button on toolbar depending on several conditions (e.g.,device width,
 * whether NTP is shown).
 */
public class ShareButtonController {
    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;

    // Toolbar manager exposes APIs for manipulating experimental button.
    private final ToolbarManager mToolbarManager;

    private final ShareUtils mShareUtils;

    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    private final Callback<ShareDelegate> mShareDelegateSupplierCallback;

    // The activity tab provider.
    private ActivityTabProvider mTabProvider;

    // The supplier for the share button on click listener.
    private ObservableSupplierImpl<OnClickListener> mShareButtonListenerSupplier =
            new ObservableSupplierImpl<>();

    private OnClickListener mOnClickListener;

    private boolean mShown;

    private boolean mIsEnabled;

    /**
     * Creates ShareButtonController object.
     * @param context The Context for retrieving resources, etc.
     * @param tabProvider The {@link ActivityTabProvider} used for accessing the tab.
     * @param toolbarManager The ToolbarManager where share button is displayed.
     * @param shareDelegateSupplier The suppliser to get a handle on the share delegate.
     * @param shareUtils The share utility functions used by this class.
     */
    public ShareButtonController(Context context, ActivityTabProvider tabProvider,
            ToolbarManager toolbarManager, ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ShareUtils shareUtils) {
        mContext = context;
        mToolbarManager = toolbarManager;
        mTabProvider = tabProvider;
        mShareUtils = shareUtils;

        mShareDelegateSupplier = shareDelegateSupplier;
        mShareDelegateSupplierCallback = this::onShareDelegateAvailable;
        mShareDelegateSupplier.addObserver(mShareDelegateSupplierCallback);
    }

    /**
     * Shows/hides share button depending on the cached button state.
     */
    public void updateButtonState() {
        updateButtonStateInternal(mIsEnabled);
    }

    /**
     * Shows/hides share button depending on the state of the current tab.
     * @param tab The tab that may be shareable.
     * @param modeState the current mode state in the toolbar.
     */
    public void updateButtonState(Tab tab, @OverviewModeState int overviewModeState) {
        // We need to check overview mode for now to ensure that we are not attempting to display
        // the share button at the same time as the identity disc. Also Overview Modes should not be
        // "shareable".
        // TODO(https://crbug.com/1041475). To be removed when we have browsing-mode specific
        // control over optional buttons.
        boolean modeStateDisabled = overviewModeState == OverviewModeState.DISABLED
                || overviewModeState == OverviewModeState.NOT_SHOWN;
        updateButtonStateInternal(modeStateDisabled && mShareUtils.shouldEnableShare(tab));
    }

    /**
     * Shows/hides share button depending on shouldEnableShare.
     * @param shouldEnableShare Whether share should be enabled.
     */
    private void updateButtonStateInternal(boolean shouldEnableShare) {
        if (!ChromeFeatureList.isInitialized()) {
            // TODO(crbug.com/1036023) Handle setting button state properly once ChromeFeatureList
            // is initialized.
            return;
        } else if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR)) {
            return;
        }

        mIsEnabled = shouldEnableShare;
        // TODO(crbug.com/1036023) add width constraints.

        boolean oldShown = mShown;
        boolean bottomShareShown = mToolbarManager.isBottomToolbarVisible()
                && BottomToolbarVariationManager.isShareButtonOnBottom();
        mShown = mIsEnabled && !bottomShareShown;

        if (oldShown == mShown) return;
        if (mShown) {
            showShareButton();
        } else if (oldShown) {
            mToolbarManager.disableExperimentalButton();
        }
    }

    /**
     * Displays share button on top toolbar.
     */
    private void showShareButton() {
        mToolbarManager.enableExperimentalButton(
                view
                -> {
                    // TODO(crbug.com/1036023) Should this assert mOnClickLister !=null;
                    if (mOnClickListener != null) mOnClickListener.onClick(view);
                },
                AppCompatResources.getDrawable(mContext, R.drawable.ic_share_white_24dp),
                R.string.share, true);
    }

    public void destroy() {
        mShareDelegateSupplier.removeObserver(mShareDelegateSupplierCallback);
    }

    private void onShareDelegateAvailable(ShareDelegate shareDelegate) {
        final OnClickListener shareButtonListener = v -> {
            RecordUserAction.record("MobileTopToolbarShareButton");
            Tab tab = mTabProvider.get();
            shareDelegate.share(tab, /*shareDirectly=*/false);
        };

        mOnClickListener = shareButtonListener;
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import android.content.Context;

import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

/**
 * Controller that handles creating and dismissed for undo snack bar when clicking on homepage
 * promo primary button.
 */
class HomepagePromoSnackbarController implements SnackbarController {
    /**
     * Data structure to carry over previous homepage state.
     */
    private static class HomepageState {
        public final boolean wasUsingNTP;
        public final boolean wasUsingDefaultUri;
        public final String originalCustomUri;

        HomepageState(boolean useNTP, boolean useDefaultUri, String customUri) {
            wasUsingNTP = useNTP;
            wasUsingDefaultUri = useDefaultUri;
            originalCustomUri = customUri;
        }
    }

    private final Context mContext;
    private final SnackbarManager mSnackbarManager;

    HomepagePromoSnackbarController(Context context, SnackbarManager snackbarManager) {
        mContext = context;
        mSnackbarManager = snackbarManager;
    }

    @Override
    public void onAction(Object actionData) {
        undo((HomepageState) actionData);
    }

    private void undo(HomepageState originState) {
        HomepageManager.getInstance().setHomepagePreferences(originState.wasUsingNTP,
                originState.wasUsingDefaultUri, originState.originalCustomUri);
    }

    /**
     * Create the undo snackbar when homepage is changed. Homepage preferences before the change
     * must be passed in to successfully undo. This function will be triggered when {@link
     * HomepagePromoController#onPrimaryButtonClicked()}
     *
     * @param useNTP True if Chrome was using NTP as homepage before the change.
     * @param useDefaultUri True if Chrome was using default URI as homepage before the change.
     * @param customUri User's custom URI before homepage changed.
     */
    void showUndoSnackbar(boolean useNTP, boolean useDefaultUri, String customUri) {
        // Action data to be passed around in #onAction.
        HomepageState origState = new HomepageState(useNTP, useDefaultUri, customUri);

        mSnackbarManager.showSnackbar(
                Snackbar.make(mContext.getString(
                                      org.chromium.chrome.R.string.homepage_promo_snackbar_message),
                                this, Snackbar.TYPE_ACTION,
                                Snackbar.UMA_HOMEPAGE_PROMO_CHANGED_UNDO)
                        .setAction(
                                mContext.getString(org.chromium.chrome.R.string.undo), origState));
    }
}

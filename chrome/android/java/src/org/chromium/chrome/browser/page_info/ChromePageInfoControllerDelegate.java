// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.site_settings.CookieControlsBridge;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Chrome's implementation of PageInfoControllerDelegate, that provides
 * Chrome-specific info to PageInfoController.
 */
public class ChromePageInfoControllerDelegate implements PageInfoControllerDelegate {
    private final WebContents mWebContents;
    private final ChromeActivity mActivity;

    public ChromePageInfoControllerDelegate(ChromeActivity activity, WebContents webContents) {
        mWebContents = webContents;
        mActivity = activity;
    }

    private Profile profile() {
        return Profile.fromWebContents(mWebContents);
    }

    @Override
    public Tracker getTracker() {
        return TrackerFactory.getTrackerForProfile(profile());
    }

    @Override
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return new ChromeAutocompleteSchemeClassifier(profile());
    }

    @Override
    public boolean cookieControlsShown() {
        return CookieControlsBridge.isCookieControlsEnabled(profile());
    }

    @Override
    public ModalDialogManager getModalDialogManager() {
        return mActivity.getModalDialogManager();
    }

    @Override
    public boolean useDarkColors() {
        return !mActivity.getNightModeStateProvider().isInNightMode();
    }
}
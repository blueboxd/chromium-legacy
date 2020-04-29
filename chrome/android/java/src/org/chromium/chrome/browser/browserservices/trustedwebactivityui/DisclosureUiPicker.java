// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureInfobar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Determines which of the versions of the "Running in Chrome" UI is displayed to the user.
 *
 * There are three:
 * * The old Infobar. (An Infobar doesn't go away until you accept it.)
 * * The new Notification. (When notifications are enabled.)
 * * The new Snackbar. (A Snackbar dismisses automatically, this one after 7 seconds.)
 */
@ActivityScope
public class DisclosureUiPicker implements NativeInitObserver {
    private final Lazy<DisclosureInfobar> mDisclosureInfobar;
    private final Lazy<DisclosureSnackbar> mDisclosureSnackbar;
    private final Lazy<DisclosureNotification> mDisclosureNotification;

    @Inject
    public DisclosureUiPicker(
            Lazy<DisclosureInfobar> disclosureInfobar,
            Lazy<DisclosureSnackbar> disclosureSnackbar,
            Lazy<DisclosureNotification> disclosureNotification,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mDisclosureInfobar = disclosureInfobar;
        mDisclosureSnackbar = disclosureSnackbar;
        mDisclosureNotification = disclosureNotification;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        // Calling get on the appropriate Lazy instance will cause Dagger to create the class.
        // The classes wire themselves up to the rest of the code in their constructors.

        // TODO(peconn): Once this feature is enabled by default and we get rid of this check, move
        // this logic into the constructor and let the Views call showIfNeeded() themselves in
        // their onFinishNativeInitialization.

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE)) {
            // TODO(https://crbug.com/1068106): Determine whether notifications (both in general and
            // the two channels we care about) are enabled. If so call disclosureNotification.get().
            mDisclosureSnackbar.get().showIfNeeded();
        } else {
            mDisclosureInfobar.get().showIfNeeded();
        }
    }
}

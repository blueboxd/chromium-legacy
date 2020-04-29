// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.view;

import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_SCOPE;

import android.content.res.Resources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import javax.inject.Inject;

import androidx.annotation.Nullable;
import dagger.Lazy;

/**
 * Implements the new "Running in Chrome" Snackbar behavior, taking over from
 * {@link DisclosureInfobar}.
 *
 * As opposed to {@link DisclosureInfobar} the Snackbar shown by this class is
 * transient (lasting 7 seconds) and only is shown at first launch (not on subsequent navigation
 * back to the verified origin).
 *
 * Thread safety: All methods should be called on the UI thread.
 */
@ActivityScope
public class DisclosureSnackbar extends DisclosureInfobar {
    // TODO(crbug.com/1068106): Once this feature is enabled by default, remove
    // TrustedWebActivityDisclosureView and simplify this class.

    private static final int DURATION_MS = 7000;

    private final Resources mResources;
    private final TrustedWebActivityModel mModel;

    private boolean mShown;

    @Inject
    DisclosureSnackbar(Resources resources,
            Lazy<SnackbarManager> snackbarManager,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        super(resources, snackbarManager, model, lifecycleDispatcher);
        mResources = resources;
        mModel = model;
    }

    @Override
    @Nullable
    protected Snackbar makeRunningInChromeInfobar(SnackbarManager.SnackbarController controller) {
        if (mShown) return null;
        mShown = true;

        String scope = mModel.get(DISCLOSURE_SCOPE);
        String text = mResources.getString(R.string.twa_running_in_chrome_v2, scope);

        int type = Snackbar.TYPE_ACTION;
        int code = Snackbar.UMA_TWA_PRIVACY_DISCLOSURE_V2;

        String action = mResources.getString(R.string.ok);

        return Snackbar.make(text, controller, type, code)
                .setAction(action, null)
                .setDuration(DURATION_MS)
                .setSingleLine(false);
    }
}

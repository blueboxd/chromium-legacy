// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.widget.Toast;

import javax.inject.Inject;

/**
 * This class enforces a quality bar on the websites shown inside Trusted Web Activities. For
 * example triggering if a link from a page on the verified origin 404s.
 *
 * The current plan for when QualityEnforcer is triggered is to finish Chrome and send a signal
 * back to the TWA shell, causing it to crash. The purpose of this is to bring TWAs in line with
 * native applications - if a native application tries to start an Activity that doesn't exist, it
 * will crash. We should hold web apps to the same standard.
 */
@ActivityScope
public class QualityEnforcer {
    @VisibleForTesting
    static final String NOTIFY = "quality_enforcement.notify";
    @VisibleForTesting
    static final String CRASH = "quality_enforcement.crash";
    private static final String KEY_CRASH_REASON = "crash_reason";
    private static final String KEY_SUCCESS = "success";

    private final ChromeActivity<?> mActivity;
    private final Verifier mVerifier;
    private final CustomTabsConnection mConnection;
    private final CustomTabsSessionToken mSessionToken;
    private final ClientPackageNameProvider mClientPackageNameProvider;

    private boolean mOriginVerified;

    private final CustomTabTabObserver mTabObserver = new CustomTabTabObserver() {
        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                    || navigation.isSameDocument()) {
                return;
            }

            String newUrl = tab.getOriginalUrl();
            if (isNavigationInScope(newUrl) && navigation.httpStatusCode() == 404) {
                String message = ContextUtils.getApplicationContext().getString(
                        R.string.twa_quality_enforcement_violation_error,
                        navigation.httpStatusCode(), newUrl);
                trigger(message);
            }
        }

        @Override
        public void onObservingDifferentTab(@NonNull Tab tab) {
            // On tab switches, update the stored verification state.
            isNavigationInScope(tab.getOriginalUrl());
        }
    };

    @Inject
    public QualityEnforcer(ChromeActivity<?> activity, TabObserverRegistrar tabObserverRegistrar,
            BrowserServicesIntentDataProvider intentDataProvider, CustomTabsConnection connection,
            Verifier verifier, ClientPackageNameProvider clientPackageNameProvider) {
        mActivity = activity;
        mVerifier = verifier;
        mConnection = connection;
        mSessionToken = intentDataProvider.getSession();
        mClientPackageNameProvider = clientPackageNameProvider;
        // Initialize the value to true before the first navigation.
        mOriginVerified = true;
        tabObserverRegistrar.registerActivityTabObserver(mTabObserver);
    }

    private void trigger(String message) {
        showErrorToast(message);

        Bundle args = new Bundle();
        args.putString(KEY_CRASH_REASON, message);
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)) {
            mConnection.sendExtraCallbackWithResult(mSessionToken, NOTIFY, args);
        } else {
            Bundle result = mConnection.sendExtraCallbackWithResult(mSessionToken, CRASH, args);
            boolean success = result != null && result.getBoolean(KEY_SUCCESS);
            if (success) mActivity.finish();
        }
    }

    private void showErrorToast(String message) {
        Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        // Only shows the toast when the TWA client app does not have installer info, i.e. install
        // via adb instead of a store.
        if (pm.getInstallerPackageName(mClientPackageNameProvider.get()) == null) {
            Toast.makeText(context, message, Toast.LENGTH_LONG).show();
        }
    }

    /*
     * Updates whether the current url is verified and returns whether the source and destination
     * are both on the verified origin.
     */
    private boolean isNavigationInScope(String newUrl) {
        if (newUrl.equals("")) return false;
        boolean wasVerified = mOriginVerified;
        Promise<Boolean> result = mVerifier.verify(newUrl);
        mOriginVerified = !result.isFulfilled() || result.getResult();
        return wasVerified && mOriginVerified;
    }
}

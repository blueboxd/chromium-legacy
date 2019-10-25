// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import android.os.Bundle;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

import androidx.browser.customtabs.CustomTabsService;

/**
 * Provides Trusted Web Activity specific behaviour for the {@link TrustedWebActivityVerifier}.
 */
public class TwaVerifierDelegate implements VerifierDelegate, Destroyable, NativeInitObserver,
        SaveInstanceStateObserver {
    /** The Digital Asset Link relationship used for Trusted Web Activities. */
    private static final int RELATIONSHIP = CustomTabsService.RELATION_HANDLE_ALL_URLS;

    /** Used in activity instance state */
    private static final String KEY_CLIENT_PACKAGE = "twaClientPackageName";

    private final CustomTabsConnection mCustomTabsConnection;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final OriginVerifier mOriginVerifier;
    private final String mClientPackageName;

    @Inject
    public TwaVerifierDelegate(
            ChromeActivity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabsConnection customTabsConnection,
            OriginVerifier.Factory originVerifierFactory,
            CustomTabActivityTabProvider tabProvider) {
        mCustomTabsConnection = customTabsConnection;
        mIntentDataProvider = intentDataProvider;

        Bundle savedInstanceState = activity.getSavedInstanceState();
        if (savedInstanceState != null) {
            mClientPackageName = savedInstanceState.getString(KEY_CLIENT_PACKAGE);
        } else {
            mClientPackageName = customTabsConnection.getClientPackageNameForSession(
                    intentDataProvider.getSession());
        }
        assert mClientPackageName != null;

        // TODO(peconn): See if we can get rid of the dependency on Web Contents.
        WebContents webContents =
                tabProvider.getTab() != null ? tabProvider.getTab().getWebContents() : null;
        mOriginVerifier =
                originVerifierFactory.create(mClientPackageName, RELATIONSHIP, webContents);

        lifecycleDispatcher.register(this);
    }

    @Override
    public String getClientPackageName() {
        return mClientPackageName;
    }

    @Override
    public boolean wasPreviouslyVerified(Origin origin) {
        return mOriginVerifier.wasPreviouslyVerified(origin);
    }

    @Override
    public Promise<Boolean> verify(Origin origin) {
        Promise<Boolean> promise = new Promise<>();
        mOriginVerifier.start((packageName, unused, verified, online) -> promise.fulfill(verified),
                origin);
        return promise;
    }

    @Override
    public void destroy() {
        // Verification may finish after activity is destroyed.
        mOriginVerifier.removeListener();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        // TODO(pshmakov): address this problem in a more general way, http://crbug.com/952221
        outState.putString(KEY_CLIENT_PACKAGE, mClientPackageName);
    }

    @Override
    public void onFinishNativeInitialization() {
        // This doesn't belong here, but doesn't deserve a separate class. Do extract it if more
        // PostMessage-related code appears.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_POST_MESSAGE)) {
            mCustomTabsConnection.resetPostMessageHandlerForSession(
                    mIntentDataProvider.getSession(), null);
        }
    }
}

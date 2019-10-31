// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

import androidx.browser.customtabs.CustomTabsService;

/**
 * Provides Trusted Web Activity specific behaviour for the {@link Verifier}.
 */
@ActivityScope
public class TwaVerifierDelegate implements VerifierDelegate, Destroyable, NativeInitObserver {
    /** The Digital Asset Link relationship used for Trusted Web Activities. */
    private static final int RELATIONSHIP = CustomTabsService.RELATION_HANDLE_ALL_URLS;

    private final CustomTabsConnection mCustomTabsConnection;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final OriginVerifier mOriginVerifier;

    @Inject
    public TwaVerifierDelegate(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabsConnection customTabsConnection,
            OriginVerifier.Factory originVerifierFactory,
            CustomTabActivityTabProvider tabProvider,
            ClientPackageNameProvider clientPackageNameProvider) {
        mCustomTabsConnection = customTabsConnection;
        mIntentDataProvider = intentDataProvider;

        // TODO(peconn): See if we can get rid of the dependency on Web Contents.
        WebContents webContents =
                tabProvider.getTab() != null ? tabProvider.getTab().getWebContents() : null;
        mOriginVerifier = originVerifierFactory.create(
                clientPackageNameProvider.get(), RELATIONSHIP, webContents);

        lifecycleDispatcher.register(this);
    }

    @Override
    public boolean wasPreviouslyVerified(Origin origin) {
        return mOriginVerifier.wasPreviouslyVerified(origin);
    }

    @Override
    public Promise<Boolean> verify(Origin origin) {
        Promise<Boolean> promise = new Promise<>();
        mOriginVerifier.start(
                (packageName, unused, verified, online) -> promise.fulfill(verified), origin);
        return promise;
    }

    @Override
    public void destroy() {
        // Verification may finish after activity is destroyed.
        mOriginVerifier.removeListener();
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

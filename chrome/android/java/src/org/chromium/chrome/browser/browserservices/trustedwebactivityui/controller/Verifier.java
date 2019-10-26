// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.content_public.browser.NavigationHandle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.inject.Inject;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Checks whether the currently seen web page belongs to a verified origin and updates the
 * {@link TrustedWebActivityModel} accordingly.
 *
 * TODO(peconn): Make this class work with both Origins and Scopes (for WebAPK unificiation).
 */
@ActivityScope
public class Verifier implements NativeInitObserver {
    private final CustomTabActivityTabProvider mTabProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final VerifierDelegate mDelegate;
    // TODO(peconn): Move this up into the coordinator.
    private final TwaRegistrar mTwaRegistrar;

    // These origins need to be verified via OriginVerifier#start, bypassing cache.
    private final Set<Origin> mOriginsToVerify = new HashSet<>();

    @Nullable private VerificationState mState;

    private final ObserverList<Runnable> mObservers = new ObserverList<>();

    @IntDef({VerificationStatus.PENDING, VerificationStatus.SUCCESS, VerificationStatus.FAILURE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VerificationStatus {
        int PENDING = 0;
        int SUCCESS = 1;
        int FAILURE = 2;
    }

    /** Represents the verification state of currently viewed web page. */
    public static class VerificationState {
        public final Origin origin;
        @VerificationStatus
        public final int status;

        public VerificationState(Origin origin, @VerificationStatus int status) {
            this.origin = origin;
            this.status = status;
        }
    }

    /** A {@link TabObserver} that checks whether we are on a verified Origin on page navigation. */
    private final TabObserver mVerifyOnPageLoadObserver = new EmptyTabObserver() {
        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            if (!navigation.hasCommitted() || !navigation.isInMainFrame()) return;
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY)) {
                assert false : "Shouldn't observe navigation when TWAs are disabled";
                return;
            }
            verifyVisitedOrigin(new Origin(navigation.getUrl()));
        }
    };

    private final CustomTabActivityTabProvider.Observer mVerifyOnTabSwitchObserver =
            new CustomTabActivityTabProvider.Observer() {
                @Override
                public void onTabSwapped(@NonNull Tab tab) {
                    // When a link with target="_blank" is followed and the user navigates back, we
                    // don't get the onDidFinishNavigation event (because the original page wasn't
                    // navigated away from, it was only ever hidden). https://crbug.com/942088
                    verifyVisitedOrigin(new Origin(tab.getUrl()));
                }
            };

    @Inject
    public Verifier(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar,
            CustomTabActivityTabProvider tabProvider,
            CustomTabIntentDataProvider intentDataProvider,
            VerifierDelegate delegate,
            TwaRegistrar twaRegistrar) {
        // TODO(peconn): Change the CustomTabIntentDataProvider to a BrowserServices... once
        // https://chromium-review.googlesource.com/c/chromium/src/+/1877600 has landed.
        mTabProvider = tabProvider;
        mTabObserverRegistrar =  tabObserverRegistrar;
        mIntentDataProvider = intentDataProvider;
        mDelegate = delegate;
        mTwaRegistrar = twaRegistrar;

        tabObserverRegistrar.registerTabObserver(mVerifyOnPageLoadObserver);
        tabProvider.addObserver(mVerifyOnTabSwitchObserver);
        lifecycleDispatcher.register(this);
    }

    /**
     * @return package name of the client app hosting this Trusted Web Activity.
     */
    public String getClientPackageName() {
        // TODO(peconn): Remove this method.
        return mDelegate.getClientPackageName();
    }

    /**
     * @return the {@link VerificationState} of the origin we are currently in.
     * Since resolving the origin requires native, returns null before native is loaded.
     * TODO(peconn): Is there any reason to distinguish between null and PENDING?
     */
    @Nullable
    public VerificationState getState() {
        return mState;
    }

    public void addVerificationObserver(Runnable observer) {
        mObservers.addObserver(observer);
    }

    public void removeVerificationObserver(Runnable observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void onFinishNativeInitialization() {
        Origin initialOrigin = new Origin(mIntentDataProvider.getUrlToLoad());
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY)) {
            mTabObserverRegistrar.unregisterTabObserver(mVerifyOnPageLoadObserver);
            updateState(initialOrigin, VerificationStatus.FAILURE);
            return;
        }

        collectTrustedOrigins(initialOrigin);
        verifyVisitedOrigin(initialOrigin);
    }

    private void collectTrustedOrigins(Origin initialOrigin) {
        mOriginsToVerify.add(initialOrigin);
        List<String> additionalOrigins =
                mIntentDataProvider.getTrustedWebActivityAdditionalOrigins();
        if (additionalOrigins != null) {
            for (String origin : additionalOrigins) {
                mOriginsToVerify.add(new Origin(origin));
            }
        }
    }

    /**
     * Returns whether the given |url| is on an Origin that the package has been previously
     * verified for.
     */
    public boolean isPageOnVerifiedOrigin(String url) {
        return mDelegate.wasPreviouslyVerified(new Origin(url));
    }

    /**
     * Verifies an arbitrary url.
     * Returns a {@link Promise<Boolean>} with boolean telling whether verification succeeded.
     */
    public Promise<Boolean> verifyOrigin(String url) {
        Origin origin = new Origin(url);
        if (mDelegate.wasPreviouslyVerified(origin)) {
            return Promise.fulfilled(true);
        }
        return mDelegate.verify(origin);
    }

    /**
     * Perform verification for the origin the user is currently on.
     */
    private void verifyVisitedOrigin(Origin origin) {
        if (mOriginsToVerify.contains(origin)) {
            // Do verification bypassing the cache.
            updateState(origin, VerificationStatus.PENDING);
            mDelegate.verify(origin).then(
                    (Callback<Boolean>) verified -> onVerificationResult(origin, verified));
        } else {
            // Look into cache only
            boolean verified = mDelegate.wasPreviouslyVerified(origin);
            updateState(origin, verified ? VerificationStatus.SUCCESS : VerificationStatus.FAILURE);
        }
    }

    /**
     * Is called as a result of a verification request to OriginVerifier. Is not called if the
     * client called |validateRelationship| before launching the TWA and we found that verification
     * in the cache.
     */
    private void onVerificationResult(Origin origin, boolean verified) {
        mOriginsToVerify.remove(origin);
        Tab tab = mTabProvider.getTab();
        boolean stillOnSameOrigin = tab != null && origin.equals(new Origin(tab.getUrl()));
        if (stillOnSameOrigin) {
            updateState(origin, verified ? VerificationStatus.SUCCESS : VerificationStatus.FAILURE);
        }
    }

    private void updateState(Origin origin, @VerificationStatus int status) {
        if (status == VerificationStatus.SUCCESS) {
            mTwaRegistrar.registerClient(mDelegate.getClientPackageName(), origin);
        }

        mState = new VerificationState(origin, status);
        for (Runnable observer : mObservers) {
            observer.run();
        }
    }
}

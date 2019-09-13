// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.permissiondelegation.NotificationPermissionUpdater;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
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

import dagger.Lazy;

/**
 * Checks whether the currently seen web page belongs to a verified origin and updates the
 * {@link TrustedWebActivityModel} accordingly.
 */
@ActivityScope
public class TrustedWebActivityVerifier implements NativeInitObserver, Destroyable,
        SaveInstanceStateObserver {
    /** The Digital Asset Link relationship used for Trusted Web Activities. */
    private final static int RELATIONSHIP = CustomTabsService.RELATION_HANDLE_ALL_URLS;

    /** Used in activity instance state */
    private static final String KEY_CLIENT_PACKAGE = "twaClientPackageName";

    private final Lazy<ClientAppDataRecorder> mClientAppDataRecorder;
    private final CustomTabsConnection mCustomTabsConnection;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final String mClientPackageName;
    private final OriginVerifier mOriginVerifier;
    private final NotificationPermissionUpdater mNotificationPermissionUpdater;

    // These origins need to be verified via OriginVerifier#start, bypassing cache.
    private final Set<Origin> mOriginsToVerify = new HashSet<>();
    // These origins have already been registered so we don't need to do so again.
    private final Set<Origin> mRegisteredOrigins = new HashSet<>();

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
    public TrustedWebActivityVerifier(Lazy<ClientAppDataRecorder> clientAppDataRecorder,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabsConnection customTabsConnection,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar,
            OriginVerifier.Factory originVerifierFactory,
            CustomTabActivityTabProvider tabProvider,
            ChromeActivity activity,
            NotificationPermissionUpdater permissionUpdater) {
        mClientAppDataRecorder = clientAppDataRecorder;
        mCustomTabsConnection = customTabsConnection;
        mIntentDataProvider = intentDataProvider;
        mTabProvider = tabProvider;
        mTabObserverRegistrar =  tabObserverRegistrar;
        mNotificationPermissionUpdater = permissionUpdater;
        Bundle savedInstanceState = activity.getSavedInstanceState();
        if (savedInstanceState != null) {
            mClientPackageName = savedInstanceState.getString(KEY_CLIENT_PACKAGE);
        } else {
            mClientPackageName = customTabsConnection.getClientPackageNameForSession(
                    intentDataProvider.getSession());
        }
        assert mClientPackageName != null;

        mOriginVerifier = originVerifierFactory.create(mClientPackageName, RELATIONSHIP);

        tabObserverRegistrar.registerTabObserver(mVerifyOnPageLoadObserver);
        tabProvider.addObserver(mVerifyOnTabSwitchObserver);
        lifecycleDispatcher.register(this);
    }

    /**
     * @return package name of the client app hosting this Trusted Web Activity.
     */
    public String getClientPackageName() {
        return mClientPackageName;
    }

    /**
     * @return the {@link VerificationState} of the origin we are currently in.
     * Since resolving the origin requires native, returns null before native is loaded.
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

        // This doesn't belong here, but doesn't deserve a separate class. Do extract it if more
        // PostMessage-related code appears.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_POST_MESSAGE)) {
            mCustomTabsConnection.resetPostMessageHandlerForSession(
                    mIntentDataProvider.getSession(), null);
        }
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
        return mOriginVerifier.wasPreviouslyVerified(new Origin(url));
    }

    /**
     * Verifies an arbitrary url.
     * Returns a {@link Promise<Boolean>} with boolean telling whether verification succeeded.
     */
    public Promise<Boolean> verifyOrigin(String url) {
        if (mOriginVerifier.wasPreviouslyVerified(new Origin(url))) {
            return Promise.fulfilled(true);
        }
        Promise<Boolean> promise = new Promise<>();
        mOriginVerifier.start((packageName, origin, verified, online) ->  promise.fulfill(verified),
                new Origin(url));
        return promise;
    }

    /**
     * Perform verification for the origin the user is currently on.
     */
    private void verifyVisitedOrigin(Origin origin) {
        if (mOriginsToVerify.contains(origin)) {
            // Do verification bypassing the cache.
            updateState(origin, VerificationStatus.PENDING);
            mOriginVerifier.start((packageName2, origin2, verified, online) ->
                    onVerificationResult(origin, verified), origin);
        } else {
            // Look into cache only
            boolean verified = mOriginVerifier.wasPreviouslyVerified(origin);
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
            registerClientAppForOrigin(origin);
        }

        mState = new VerificationState(origin, status);
        for (Runnable observer : mObservers) {
            observer.run();
        }
    }

    @Override
    public void destroy() {
        // Verification may finish after activity is destroyed.
        mOriginVerifier.removeListener();
    }

    /**
     * Registers to various stores that the client app is linked with the origin.
     *
     * We do this here, when the Trusted Web Activity UI is shown instead of in OriginVerifier when
     * verification completes because when an origin is being verified, we don't know whether it is
     * for the purposes of Trusted Web Activities or for Post Message (where this behaviour is not
     * required).
     *
     * Additionally we do it on every page navigation because an app can be verified for more than
     * one Origin, eg:
     * 1) App verifies with https://www.myfirsttwa.com/.
     * 2) App verifies with https://www.mysecondtwa.com/.
     * 3) App launches a TWA to https://www.myfirsttwa.com/.
     * 4) App navigates to https://www.mysecondtwa.com/.
     *
     * At step 2, we don't know why the app is verifying with that origin (it could be for TWAs or
     * for PostMessage). Only at step 4 do we know that Chrome should associate the browsing data
     * for that origin with that app.
     */
    private void registerClientAppForOrigin(Origin origin) {
        if (mRegisteredOrigins.contains(origin)) return;

        // Register that we should wipe data for this origin when the client app is uninstalled.
        mClientAppDataRecorder.get().register(mClientPackageName, origin);
        // Register that we trust the client app to forward notifications from this origin to.
        TrustedWebActivityClient.registerClient(
                ContextUtils.getApplicationContext(), origin, mClientPackageName);
        // Update Chrome's notification permission for the website to that of the client app.
        mNotificationPermissionUpdater.onOriginVerified(origin, mClientPackageName);

        mRegisteredOrigins.add(origin);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        // TODO(pshmakov): address this problem in a more general way, http://crbug.com/952221
        outState.putString(KEY_CLIENT_PACKAGE, mClientPackageName);
    }
}

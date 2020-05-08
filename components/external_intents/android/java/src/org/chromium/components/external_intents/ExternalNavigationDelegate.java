// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.NonNull;

import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * A delegate for the class responsible for navigating to external applications from Chrome. Used
 * by {@link ExternalNavigationHandler}.
 */
public interface ExternalNavigationDelegate {
    /**
     * Returns the Activity with which this delegate is associated, or null if there
     * is no such Activity at the time of invocation.
     */
    Activity getActivityContext();

    /**
     * Determine if Chrome is the default or only handler for a given intent. If true, Chrome
     * will handle the intent when started.
     */
    boolean willChromeHandleIntent(Intent intent);

    /**
     * Returns whether to disable forwarding URL requests to external intents for the passed-in URL.
     */
    boolean shouldDisableExternalIntentRequestsForUrl(String url);

    /**
     * Returns whether the embedder has custom integration with InstantApps (most embedders will not
     * have any such integration).
     */
    boolean handlesInstantAppLaunchingInternally();

    /**
     * Start an activity for the intent. Used for intents that must be handled externally.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     */
    void startActivity(Intent intent, boolean proxy);

    /**
     * Start an activity for the intent. Used for intents that may be handled internally or
     * externally. If the user chooses to handle the intent internally, this routine must return
     * false.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     */
    boolean startActivityIfNeeded(Intent intent, boolean proxy);

    /**
     * Display a dialog warning the user that they may be leaving Chrome by starting this
     * intent. Give the user the opportunity to cancel the action. And if it is canceled, a
     * navigation will happen in Chrome. Catches BadTokenExceptions caused by showing the dialog
     * on certain devices. (crbug.com/782602)
     * @param intent The intent for external application that will be sent.
     * @param referrerUrl The referrer for the current navigation.
     * @param fallbackUrl The URL to load if the user doesn't proceed with external intent.
     * @param needsToCloseTab Whether the current tab has to be closed after the intent is sent.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents.
     * @return True if the function returned error free, false if it threw an exception.
     */
    boolean startIncognitoIntent(Intent intent, String referrerUrl, String fallbackUrl,
            boolean needsToCloseTab, boolean proxy);

    /**
     * Handle the incognito intent by loading it as a URL in the embedder, using the fallbackUrl if
     * the intent URL cannot be handled by the embedder.
     * @param intent The intent to be handled by the embedder.
     * @param referrerUrl The referrer for the current navigation.
     * @param fallbackUrl The fallback URL to load if the intent cannot be handled by the embedder.
     * @return The OverrideUrlLoadingResult for the action taken by the embedder.
     */
    @OverrideUrlLoadingResult
    int handleIncognitoIntentTargetingSelf(Intent intent, String referrerUrl, String fallbackUrl);

    /**
     * Loads a URL as specified by |loadUrlParams| if possible. May fail in exceptional conditions
     * (e.g., if there is no valid tab).
     * @param loadUrlParams parameters of the URL to be loaded
     */
    void loadUrlIfPossible(LoadUrlParams loadUrlParams);

    /** Adds a window id to the intent, if necessary. */
    void maybeSetWindowId(Intent intent);

    /** Records the pending referrer if desired. */
    void maybeSetPendingReferrer(Intent intent, @NonNull String referrerUrl);

    /**
     * Adjusts any desired extras related to intents to instant apps based on the value of
     * |insIntentToInstantApp}.
     */
    void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp);

    /** Invoked for intents with user gestures and records the user gesture if desired. */
    void maybeSetUserGesture(Intent intent);

    /**
     * Records the pending incognito URL if desired. Called only if the
     * navigation is occurring in the context of incognito mode.
     */
    void maybeSetPendingIncognitoUrl(Intent intent);

    /**
     * Determine if the Chrome app is in the foreground.
     */
    boolean isChromeAppInForeground();

    /**
     * Check if the URL should be handled by an instant app, or kick off an async request for an
     * instant app banner.
     * @param url The current URL.
     * @param referrerUrl The referrer URL.
     * @param isIncomingRedirect Whether we are handling an incoming redirect to an instant app.
     * @param isSerpReferrer whether the referrer is the SERP.
     * @return Whether we launched an instant app.
     */
    boolean maybeLaunchInstantApp(
            String url, String referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer);

    /**
     * @return The WindowAndroid instance associated with this delegate instance.
     */
    WindowAndroid getWindowAndroid();

    /**
     * @return The WebContents instance associated with this delegate instance.
     */
    WebContents getWebContents();

    /**
     * @return Whether this delegate has a valid tab available.
     */
    boolean hasValidTab();

    /**
     * @return whether this delegate supports creation of new tabs. If this method returns false,
     * all URLs loaded by ExternalNavigationHandler will be loaded in the current tab and
     * loadUrlInNewTab() will never be invoked.
     */
    boolean supportsCreatingNewTabs();

    /**
     * Loads |url| in a new tab.
     * @param url The URL to load.
     * @param launchIncognito whether the new tab should be incognito.
     */
    void loadUrlInNewTab(final String url, final boolean launchIncognito);

    /**
     * @return whether it's possible to load a URL in the current tab.
     */
    boolean canLoadUrlInCurrentTab();

    /* Invoked when the tab associated with this delegate should be closed. */
    void closeTab();

    /* Returns whether whether the tab associated with this delegate is incognito. */
    boolean isIncognito();

    /**
     * @param intent The intent to launch.
     * @return Whether the Intent points to an app that we trust and that launched Chrome.
     */
    boolean isIntentForTrustedCallingApp(Intent intent);

    /**
     * @param intent The intent to launch.
     * @return Whether the Intent points to an instant app.
     */
    boolean isIntentToInstantApp(Intent intent);

    /**
     * @param intent The intent to launch
     * @return Whether the Intent points to Autofill Assistant
     */
    boolean isIntentToAutofillAssistant(Intent intent);

    /**
     * @param packageName The package to check.
     * @return Whether the package is a valid WebAPK package.
     */
    boolean isValidWebApk(String packageName);

    /**
     * Gives the embedder a chance to handle the intent via the autofill assistant.
     */
    boolean handleWithAutofillAssistant(ExternalNavigationParams params, Intent targetIntent,
            String browserFallbackUrl, boolean isGoogleReferrer);
}

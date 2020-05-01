// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.Intent;
import android.content.pm.ResolveInfo;

import androidx.annotation.NonNull;

import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * A delegate for the class responsible for navigating to external applications from Chrome. Used
 * by {@link ExternalNavigationHandler}.
 */
public interface ExternalNavigationDelegate {
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
     * Returns the number of specialized intent handlers in {@params infos}. Specialized intent
     * handlers are intent handlers which handle only a few URLs (e.g. google maps or youtube).
     */
    int countSpecializedHandlers(List<ResolveInfo> infos);

    /**
     * Returns the subset of {@params infos} that are specialized intent handlers.
     */
    ArrayList<String> getSpecializedHandlers(List<ResolveInfo> infos);

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
     * Trigger a UI affordance that will ask the user to grant file access.  After the access
     * has been granted or denied, continue loading the specified file URL.
     *
     * @param intent The intent to continue loading the file URL.
     * @param referrerUrl The HTTP referrer URL.
     * @param needsToCloseTab Whether this action should close the current tab.
     */
    void startFileIntent(Intent intent, String referrerUrl, boolean needsToCloseTab);

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
    public boolean hasValidTab();

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

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.SystemClock;
import android.provider.Browser;
import android.text.TextUtils;
import android.util.Pair;
import android.webkit.WebView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.URI;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * Logic related to the URL overriding/intercepting functionality.
 * This feature allows Chrome to convert certain navigations to Android Intents allowing
 * applications like Youtube to direct users clicking on a http(s) link to their native app.
 */
public class ExternalNavigationHandler {
    private static final String TAG = "UrlHandler";

    // Enables debug logging on a local build.
    private static final boolean DEBUG = false;

    private static final String WTAI_URL_PREFIX = "wtai://wp/";
    private static final String WTAI_MC_URL_PREFIX = "wtai://wp/mc;";

    private static final String PLAY_PACKAGE_PARAM = "id";
    private static final String PLAY_REFERRER_PARAM = "referrer";
    private static final String PLAY_APP_PATH = "/store/apps/details";
    private static final String PLAY_HOSTNAME = "play.google.com";

    @VisibleForTesting
    public static final String EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url";

    // An extra that may be specified on an intent:// URL that contains an encoded value for the
    // referrer field passed to the market:// URL in the case where the app is not present.
    @VisibleForTesting
    public static final String EXTRA_MARKET_REFERRER = "market_referrer";

    // These values are persisted in histograms. Please do not renumber. Append only.
    @IntDef({AiaIntent.FALLBACK_USED, AiaIntent.SERP, AiaIntent.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AiaIntent {
        int FALLBACK_USED = 0;
        int SERP = 1;
        int OTHER = 2;

        int NUM_ENTRIES = 3;
    }

    // Standard Activity Actions, as defined by:
    // https://developer.android.com/reference/android/content/Intent.html#standard-activity-actions
    // These values are persisted in histograms. Please do not renumber.
    @IntDef({StandardActions.MAIN, StandardActions.VIEW, StandardActions.ATTACH_DATA,
            StandardActions.EDIT, StandardActions.PICK, StandardActions.CHOOSER,
            StandardActions.GET_CONTENT, StandardActions.DIAL, StandardActions.CALL,
            StandardActions.SEND, StandardActions.SENDTO, StandardActions.ANSWER,
            StandardActions.INSERT, StandardActions.DELETE, StandardActions.RUN,
            StandardActions.SYNC, StandardActions.PICK_ACTIVITY, StandardActions.SEARCH,
            StandardActions.WEB_SEARCH, StandardActions.FACTORY_TEST, StandardActions.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface StandardActions {
        int MAIN = 0;
        int VIEW = 1;
        int ATTACH_DATA = 2;
        int EDIT = 3;
        int PICK = 4;
        int CHOOSER = 5;
        int GET_CONTENT = 6;
        int DIAL = 7;
        int CALL = 8;
        int SEND = 9;
        int SENDTO = 10;
        int ANSWER = 11;
        int INSERT = 12;
        int DELETE = 13;
        int RUN = 14;
        int SYNC = 15;
        int PICK_ACTIVITY = 16;
        int SEARCH = 17;
        int WEB_SEARCH = 18;
        int FACTORY_TEST = 19;
        int OTHER = 20;

        int NUM_ENTRIES = 21;
    }

    @VisibleForTesting
    public static final String INTENT_ACTION_HISTOGRAM =
            "Android.Intent.OverrideUrlLoadingIntentAction";

    private final ExternalNavigationDelegate mDelegate;

    /**
     * Result types for checking if we should override URL loading.
     * NOTE: this enum is used in UMA, do not reorder values. Changes should be append only.
     * Values should be numerated from 0 and can't have gaps.
     */
    @IntDef({OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
            OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB,
            OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
            OverrideUrlLoadingResult.NO_OVERRIDE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OverrideUrlLoadingResult {
        /* We should override the URL loading and launch an intent. */
        int OVERRIDE_WITH_EXTERNAL_INTENT = 0;
        /* We should override the URL loading and clobber the current tab. */
        int OVERRIDE_WITH_CLOBBERING_TAB = 1;
        /* We should override the URL loading.  The desired action will be determined
         * asynchronously (e.g. by requiring user confirmation). */
        int OVERRIDE_WITH_ASYNC_ACTION = 2;
        /* We shouldn't override the URL loading. */
        int NO_OVERRIDE = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * Constructs a new instance of {@link ExternalNavigationHandler}, using the injected
     * {@link ExternalNavigationDelegate}.
     */
    public ExternalNavigationHandler(ExternalNavigationDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Determines whether the URL needs to be sent as an intent to the system,
     * and sends it, if appropriate.
     * @return Whether the URL generated an intent, caused a navigation in
     *         current tab, or wasn't handled at all.
     */
    public @OverrideUrlLoadingResult int shouldOverrideUrlLoading(ExternalNavigationParams params) {
        if (DEBUG) Log.i(TAG, "shouldOverrideUrlLoading called on " + params.getUrl());
        Intent targetIntent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            targetIntent = Intent.parseUri(params.getUrl(), Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", params.getUrl(), ex);
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        String browserFallbackUrl =
                IntentUtils.safeGetStringExtra(targetIntent, EXTRA_BROWSER_FALLBACK_URL);
        if (browserFallbackUrl != null
                && !UrlUtilities.isValidForIntentFallbackNavigation(browserFallbackUrl)) {
            browserFallbackUrl = null;
        }
        long time = SystemClock.elapsedRealtime();
        @OverrideUrlLoadingResult
        int result = shouldOverrideUrlLoadingInternal(params, targetIntent, browserFallbackUrl);
        RecordHistogram.recordTimesHistogram(
                "Android.StrictMode.OverrideUrlLoadingTime", SystemClock.elapsedRealtime() - time);

        if (result != OverrideUrlLoadingResult.NO_OVERRIDE) {
            int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
            boolean isFormSubmit = pageTransitionCore == PageTransition.FORM_SUBMIT;
            boolean isRedirectFromFormSubmit = isFormSubmit && params.isRedirect();
            if (isRedirectFromFormSubmit) {
                RecordHistogram.recordBooleanHistogram(
                        "Android.Intent.LaunchExternalAppFormSubmitHasUserGesture",
                        params.hasUserGesture());
            }
        } else if (result == OverrideUrlLoadingResult.NO_OVERRIDE && browserFallbackUrl != null
                && (params.getRedirectHandler() == null
                        // For instance, if this is a chained fallback URL, we ignore it.
                        || !params.getRedirectHandler().shouldNotOverrideUrlLoading())) {
            result = handleFallbackUrl(params, targetIntent, browserFallbackUrl);
        }
        if (DEBUG) printDebugShouldOverrideUrlLoadingResult(result);
        return result;
    }

    private @OverrideUrlLoadingResult int handleFallbackUrl(
            ExternalNavigationParams params, Intent targetIntent, String browserFallbackUrl) {
        if (mDelegate.isIntentToInstantApp(targetIntent)) {
            RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                    AiaIntent.FALLBACK_USED, AiaIntent.NUM_ENTRIES);
        }
        // Launch WebAPK if it can handle the URL.
        try {
            Intent intent = Intent.parseUri(browserFallbackUrl, Intent.URI_INTENT_SCHEME);
            sanitizeQueryIntentActivitiesIntent(intent);
            List<ResolveInfo> resolvingInfos = mDelegate.queryIntentActivities(intent);
            if (!isAlreadyInTargetWebApk(resolvingInfos, params)
                    && launchWebApkIfSoleIntentHandler(resolvingInfos, intent)) {
                return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
            }
        } catch (Exception e) {
            if (DEBUG) Log.i(TAG, "Could not parse fallback url as intent");
        }
        return clobberCurrentTabWithFallbackUrl(browserFallbackUrl, params);
    }

    private void printDebugShouldOverrideUrlLoadingResult(int result) {
        String resultString;
        switch (result) {
            case OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT:
                resultString = "OVERRIDE_WITH_EXTERNAL_INTENT";
                break;
            case OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB:
                resultString = "OVERRIDE_WITH_CLOBBERING_TAB";
                break;
            case OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION:
                resultString = "OVERRIDE_WITH_ASYNC_ACTION";
                break;
            case OverrideUrlLoadingResult.NO_OVERRIDE: // Fall through.
            default:
                resultString = "NO_OVERRIDE";
                break;
        }
        Log.i(TAG, "shouldOverrideUrlLoading result: " + resultString);
    }

    private boolean resolversSubsetOf(List<ResolveInfo> infos, List<ResolveInfo> container) {
        if (container == null) return false;
        HashSet<ComponentName> containerSet = new HashSet<>();
        for (ResolveInfo info : container) {
            containerSet.add(
                    new ComponentName(info.activityInfo.packageName, info.activityInfo.name));
        }
        for (ResolveInfo info : infos) {
            if (!containerSet.contains(
                        new ComponentName(info.activityInfo.packageName, info.activityInfo.name))) {
                return false;
            }
        }
        return true;
    }

    /**
     * http://crbug.com/441284 : Disallow firing external intent while Chrome is in the background.
     */
    private boolean blockExternalNavWhileBackgrounded(ExternalNavigationParams params) {
        if (params.isApplicationMustBeInForeground() && !mDelegate.isChromeAppInForeground()) {
            if (DEBUG) Log.i(TAG, "Chrome is not in foreground");
            return true;
        }
        return false;
    }

    /** http://crbug.com/464669 : Disallow firing external intent from background tab. */
    private boolean blockExternalNavFromBackgroundTab(ExternalNavigationParams params) {
        if (params.isBackgroundTabNavigation()) {
            if (DEBUG) Log.i(TAG, "Navigation in background tab");
            return true;
        }
        return false;
    }

    /**
     * http://crbug.com/164194 . A navigation forwards or backwards should never trigger the intent
     * picker.
     */
    private boolean ignoreBackForwardNav(ExternalNavigationParams params) {
        if ((params.getPageTransition() & PageTransition.FORWARD_BACK) != 0) {
            if (DEBUG) Log.i(TAG, "Forward or back navigation");
            return true;
        }
        return false;
    }

    /** http://crbug.com/605302 : Allow Chrome to handle all pdf file downloads. */
    private boolean isInternalPdfDownload(
            boolean isExternalProtocol, ExternalNavigationParams params) {
        if (!isExternalProtocol && mDelegate.isPdfDownload(params.getUrl())) {
            if (DEBUG) Log.i(TAG, "PDF downloads are now handled by Chrome");
            return true;
        }
        return false;
    }

    /**
     * If accessing a file URL, ensure that the user has granted the necessary file access
     * to Chrome.
     */
    private boolean startFileIntentIfNecessary(
            ExternalNavigationParams params, Intent targetIntent) {
        if (params.getUrl().startsWith(UrlConstants.FILE_URL_SHORT_PREFIX)
                && mDelegate.shouldRequestFileAccess(params.getUrl())) {
            mDelegate.startFileIntent(targetIntent, params.getReferrerUrl(),
                    params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent());
            if (DEBUG) Log.i(TAG, "Requesting filesystem access");
            return true;
        }
        return false;
    }

    private boolean isTypedRedirectToExternalProtocol(
            ExternalNavigationParams params, int pageTransitionCore, boolean isExternalProtocol) {
        boolean isTyped = (pageTransitionCore == PageTransition.TYPED)
                || ((params.getPageTransition() & PageTransition.FROM_ADDRESS_BAR) != 0);
        return isTyped && params.isRedirect() && isExternalProtocol;
    }

    /**
     * http://crbug.com/659301: Don't stay in Chrome for Custom Tabs redirecting to Instant Apps.
     */
    private boolean handleCCTRedirectsToInstantApps(ExternalNavigationParams params,
            boolean isExternalProtocol, boolean incomingIntentRedirect) {
        RedirectHandler handler = params.getRedirectHandler();
        if (handler == null) return false;
        if (handler.isFromCustomTabIntent() && !isExternalProtocol && incomingIntentRedirect
                && !handler.shouldNavigationTypeStayInApp()
                && mDelegate.maybeLaunchInstantApp(
                        params.getUrl(), params.getReferrerUrl(), true)) {
            if (DEBUG) {
                Log.i(TAG, "Launching redirect to an instant app");
            }
            return true;
        }
        return false;
    }

    private boolean redirectShouldStayInChrome(
            ExternalNavigationParams params, boolean isExternalProtocol, Intent targetIntent) {
        RedirectHandler handler = params.getRedirectHandler();
        if (handler == null) return false;
        boolean shouldStayInApp = handler.shouldStayInApp(
                isExternalProtocol, mDelegate.isIntentForTrustedCallingApp(targetIntent));
        if (shouldStayInApp || handler.shouldNotOverrideUrlLoading()) {
            if (DEBUG) Log.i(TAG, "RedirectHandler decision");
            return true;
        }
        return false;
    }

    /** Wrapper of check against the feature to support overriding for testing. */
    @VisibleForTesting
    public boolean blockExternalFormRedirectsWithoutGesture() {
        return ExternalIntentsFeatureList.isEnabled(
                ExternalIntentsFeatureList.INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE);
    }

    /**
     * http://crbug.com/149218: We want to show the intent picker for ordinary links, providing
     * the link is not an incoming intent from another application, unless it's a redirect.
     */
    private boolean preferToShowIntentPicker(ExternalNavigationParams params,
            int pageTransitionCore, boolean isExternalProtocol, boolean isFormSubmit,
            boolean linkNotFromIntent, boolean incomingIntentRedirect) {
        // http://crbug.com/169549 : If you type in a URL that then redirects in server side to a
        // link that cannot be rendered by the browser, we want to show the intent picker.
        if (isTypedRedirectToExternalProtocol(params, pageTransitionCore, isExternalProtocol)) {
            return true;
        }
        // http://crbug.com/181186: We need to show the intent picker when we receive a redirect
        // following a form submit.
        boolean isRedirectFromFormSubmit = isFormSubmit && params.isRedirect();

        if (!linkNotFromIntent && !incomingIntentRedirect && !isRedirectFromFormSubmit) {
            if (DEBUG) Log.i(TAG, "Incoming intent (not a redirect)");
            return false;
        }
        // http://crbug.com/839751: Require user gestures for form submits to external
        //                          protocols.
        // TODO(tedchoc): Remove the ChromeFeatureList check once we verify this change does
        //                not break the world.
        if (isRedirectFromFormSubmit && !incomingIntentRedirect && !params.hasUserGesture()
                && blockExternalFormRedirectsWithoutGesture()) {
            if (DEBUG) {
                Log.i(TAG,
                        "Incoming form intent attempting to redirect without "
                                + "user gesture");
            }
            return false;
        }
        // http://crbug/331571 : Do not override a navigation started from user typing.
        if (params.getRedirectHandler() != null
                && params.getRedirectHandler().isNavigationFromUserTyping()) {
            if (DEBUG) Log.i(TAG, "Navigation from user typing");
            return false;
        }
        return true;
    }

    /**
     * http://crbug.com/159153: Don't override navigation from a chrome:* url to http or https. For
     * example when clicking a link in bookmarks or most visited. When navigating from such a page,
     * there is clear intent to complete the navigation in Chrome.
     */
    private boolean isLinkFromChromeInternalPage(ExternalNavigationParams params) {
        if (params.getReferrerUrl() == null) return false;
        if (params.getReferrerUrl().startsWith(UrlConstants.CHROME_URL_PREFIX)
                && (params.getUrl().startsWith(UrlConstants.HTTP_URL_PREFIX)
                        || params.getUrl().startsWith(UrlConstants.HTTPS_URL_PREFIX))) {
            if (DEBUG) Log.i(TAG, "Link from an internal chrome:// page");
            return true;
        }
        return false;
    }

    private boolean handleWtaiMcProtocol(ExternalNavigationParams params) {
        if (!params.getUrl().startsWith(WTAI_MC_URL_PREFIX)) return false;
        // wtai://wp/mc;number
        // number=string(phone-number)
        mDelegate.startActivity(
                new Intent(Intent.ACTION_VIEW,
                        Uri.parse(WebView.SCHEME_TEL
                                + params.getUrl().substring(WTAI_MC_URL_PREFIX.length()))),
                false);
        if (DEBUG) Log.i(TAG, "wtai:// link handled");
        RecordUserAction.record("Android.PhoneIntent");
        return true;
    }

    private boolean isUnhandledWtaiProtocol(ExternalNavigationParams params) {
        if (!params.getUrl().startsWith(WTAI_URL_PREFIX)) return false;
        if (DEBUG) Log.i(TAG, "Unsupported wtai:// link");
        return true;
    }

    /**
     * The "about:", "chrome:", "chrome-native:", and "devtools:" schemes
     * are internal to the browser; don't want these to be dispatched to other apps.
     */
    private boolean hasInternalScheme(
            ExternalNavigationParams params, Intent targetIntent, boolean hasIntentScheme) {
        String url;
        if (hasIntentScheme) {
            // TODO(https://crbug.com/783819): When this function is converted to GURL, we should
            // also call fixUpUrl on this user-provided URL as the fixed-up URL is what we would end
            // up navigating to.
            url = targetIntent.getDataString();
            if (url == null) return false;
        } else {
            url = params.getUrl();
        }
        if (url.startsWith(ContentUrlConstants.ABOUT_SCHEME)
                || url.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX)
                || url.startsWith(UrlConstants.DEVTOOLS_URL_SHORT_PREFIX)) {
            if (DEBUG) Log.i(TAG, "Navigating to a chrome-internal page");
            return true;
        }
        return false;
    }

    /** The "content:" scheme is disabled in Clank. Do not try to start an activity. */
    private boolean hasContentScheme(
            ExternalNavigationParams params, Intent targetIntent, boolean hasIntentScheme) {
        String url;
        if (hasIntentScheme) {
            url = targetIntent.getDataString();
            if (url == null) return false;
        } else {
            url = params.getUrl();
        }
        if (!url.startsWith(UrlConstants.CONTENT_URL_SHORT_PREFIX)) return false;
        if (DEBUG) Log.i(TAG, "Navigation to content: URL");
        return true;
    }

    /**
     * Special case - It makes no sense to use an external application for a YouTube
     * pairing code URL, since these match the current tab with a device (Chromecast
     * or similar) it is supposed to be controlling. Using a different application
     * that isn't expecting this (in particular YouTube) doesn't work.
     */
    private boolean isYoutubePairingCode(ExternalNavigationParams params) {
        // TODO(https://crbug.com/1009539): Replace this regex with proper URI parsing.
        if (params.getUrl().matches(".*youtube\\.com(\\/.*)?\\?(.+&)?pairingCode=[^&].+")) {
            if (DEBUG) Log.i(TAG, "YouTube URL with a pairing code");
            return true;
        }
        return false;
    }

    private boolean externalIntentRequestsDisabledForUrl(ExternalNavigationParams params) {
        // TODO(changwan): check if we need to handle URL even when external intent is off.
        if (CommandLine.getInstance().hasSwitch(
                    ExternalIntentsSwitches.DISABLE_EXTERNAL_INTENT_REQUESTS)) {
            Log.w(TAG, "External intent handling is disabled by a command-line flag.");
            return true;
        }

        if (mDelegate.shouldDisableExternalIntentRequestsForUrl(params.getUrl())) {
            if (DEBUG) Log.i(TAG, "Delegate disables external intent requests for URL.");
            return true;
        }
        return false;
    }

    /**
     * If the intent can't be resolved, we should fall back to the browserFallbackUrl, or try to
     * find the app on the market if no fallback is provided.
     */
    private int handleUnresolvableIntent(
            ExternalNavigationParams params, Intent targetIntent, String browserFallbackUrl) {
        // Fallback URL will be handled by the caller of shouldOverrideUrlLoadingInternal.
        if (browserFallbackUrl != null) return OverrideUrlLoadingResult.NO_OVERRIDE;
        if (targetIntent.getPackage() != null) return handleWithMarketIntent(params, targetIntent);

        if (DEBUG) Log.i(TAG, "Could not find an external activity to use");
        return OverrideUrlLoadingResult.NO_OVERRIDE;
    }

    private @OverrideUrlLoadingResult int handleWithMarketIntent(
            ExternalNavigationParams params, Intent intent) {
        String marketReferrer = IntentUtils.safeGetStringExtra(intent, EXTRA_MARKET_REFERRER);
        if (TextUtils.isEmpty(marketReferrer)) {
            marketReferrer = ContextUtils.getApplicationContext().getPackageName();
        }
        return sendIntentToMarket(intent.getPackage(), marketReferrer, params);
    }

    private boolean maybeSetSmsPackage(Intent targetIntent) {
        final Uri uri = targetIntent.getData();
        if (targetIntent.getPackage() == null && uri != null
                && UrlConstants.SMS_SCHEME.equals(uri.getScheme())) {
            List<ResolveInfo> resolvingInfos = mDelegate.queryIntentActivities(targetIntent);
            targetIntent.setPackage(getDefaultSmsPackageName(resolvingInfos));
            return true;
        }
        return false;
    }

    private void maybeRecordPhoneIntentMetrics(Intent targetIntent) {
        final Uri uri = targetIntent.getData();
        if (uri != null && UrlConstants.TEL_SCHEME.equals(uri.getScheme())
                || (Intent.ACTION_DIAL.equals(targetIntent.getAction()))
                || (Intent.ACTION_CALL.equals(targetIntent.getAction()))) {
            RecordUserAction.record("Android.PhoneIntent");
        }
    }

    /**
     * In incognito mode, links that can be handled within the browser should just do so,
     * without asking the user.
     */
    private boolean shouldStayInIncognito(
            ExternalNavigationParams params, boolean isExternalProtocol) {
        if (params.isIncognito() && !isExternalProtocol) {
            if (DEBUG) Log.i(TAG, "Stay incognito");
            return true;
        }
        return false;
    }

    private boolean fallBackToHandlingWithInstantApp(ExternalNavigationParams params,
            boolean incomingIntentRedirect, boolean linkNotFromIntent) {
        if (incomingIntentRedirect
                && mDelegate.maybeLaunchInstantApp(
                        params.getUrl(), params.getReferrerUrl(), true)) {
            if (DEBUG) Log.i(TAG, "Launching instant Apps redirect");
            return true;
        } else if (linkNotFromIntent && !params.isIncognito()
                && mDelegate.maybeLaunchInstantApp(
                        params.getUrl(), params.getReferrerUrl(), false)) {
            if (DEBUG) Log.i(TAG, "Launching instant Apps link");
            return true;
        }
        return false;
    }

    /**
     * This is the catch-all path for any intent that Chrome can handle that doesn't have a
     * specialized external app handling it.
     */
    private @OverrideUrlLoadingResult int fallBackToHandlingInChrome() {
        if (DEBUG) Log.i(TAG, "No specialized handler for URL");
        return OverrideUrlLoadingResult.NO_OVERRIDE;
    }

    /**
     * Current URL has at least one specialized handler available. For navigations
     * within the same host, keep the navigation inside the browser unless the set of
     * available apps to handle the new navigation is different. http://crbug.com/463138
     */
    private boolean shouldStayWithinHost(ExternalNavigationParams params, boolean isLink,
            boolean isFormSubmit, List<ResolveInfo> resolvingInfos, boolean isExternalProtocol) {
        if (isExternalProtocol) return false;

        // TODO(https://crbug.com/1009539): Replace this host parsing with a UrlUtilities or GURL
        //   function call.
        String delegatePreviousUrl = mDelegate.getPreviousUrl();
        String previousUriString =
                delegatePreviousUrl != null ? delegatePreviousUrl : params.getReferrerUrl();
        if (previousUriString == null || (!isLink && !isFormSubmit)) return false;

        URI currentUri;
        URI previousUri;

        try {
            currentUri = new URI(params.getUrl());
            previousUri = new URI(previousUriString);
        } catch (Exception e) {
            return false;
        }

        if (currentUri == null || previousUri == null
                || !TextUtils.equals(currentUri.getHost(), previousUri.getHost())) {
            return false;
        }

        Intent previousIntent;
        try {
            previousIntent = Intent.parseUri(previousUriString, Intent.URI_INTENT_SCHEME);
        } catch (Exception e) {
            return false;
        }

        if (previousIntent != null
                && resolversSubsetOf(
                        resolvingInfos, mDelegate.queryIntentActivities(previousIntent))) {
            if (DEBUG) Log.i(TAG, "Same host, no new resolvers");
            return true;
        }
        return false;
    }

    /**
     * For security reasons, we disable all intent:// URLs to Instant Apps that are not coming from
     * SERP.
     */
    private boolean preventDirectInstantAppsIntent(
            boolean isDirectInstantAppsIntent, boolean shouldProxyForInstantApps) {
        if (!isDirectInstantAppsIntent || shouldProxyForInstantApps) return false;
        if (DEBUG) Log.i(TAG, "Intent URL to an Instant App");
        RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                AiaIntent.OTHER, AiaIntent.NUM_ENTRIES);
        return true;
    }

    /**
     * Prepare the intent to be sent. This function does not change the filtering for the intent,
     * so the list if resolveInfos for the intent will be the same before and after this function.
     */
    private void prepareExternalIntent(Intent targetIntent, ExternalNavigationParams params,
            List<ResolveInfo> resolvingInfos, boolean shouldProxyForInstantApps) {
        // Set the Browser application ID to us in case the user chooses Chrome
        // as the app.  This will make sure the link is opened in the same tab
        // instead of making a new one.
        targetIntent.putExtra(Browser.EXTRA_APPLICATION_ID,
                ContextUtils.getApplicationContext().getPackageName());
        if (params.isOpenInNewTab()) targetIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        targetIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Ensure intents re-target potential caller activity when we run in CCT mode.
        targetIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        mDelegate.maybeSetWindowId(targetIntent);
        mDelegate.maybeRecordAppHandlersInIntent(targetIntent, resolvingInfos);

        if (params.getReferrerUrl() != null) {
            mDelegate.maybeSetPendingReferrer(targetIntent, params.getReferrerUrl());
        }

        if (params.isIncognito()) mDelegate.maybeSetPendingIncognitoUrl(targetIntent);

        mDelegate.maybeAdjustInstantAppExtras(targetIntent, shouldProxyForInstantApps);

        if (shouldProxyForInstantApps) {
            RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                    AiaIntent.SERP, AiaIntent.NUM_ENTRIES);
        }

        if (params.hasUserGesture()) mDelegate.maybeSetUserGesture(targetIntent);
    }

    private @OverrideUrlLoadingResult int handleExternalIncognitoIntent(Intent targetIntent,
            ExternalNavigationParams params, String browserFallbackUrl,
            boolean shouldProxyForInstantApps) {
        // This intent may leave Chrome. Warn the user that incognito does not carry over
        // to apps out side of Chrome.
        if (mDelegate.startIncognitoIntent(targetIntent, params.getReferrerUrl(),
                    browserFallbackUrl,
                    params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(),
                    shouldProxyForInstantApps)) {
            if (DEBUG) Log.i(TAG, "Incognito navigation out");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION;
        }
        if (DEBUG) Log.i(TAG, "Failed to show incognito alert dialog.");
        return OverrideUrlLoadingResult.NO_OVERRIDE;
    }

    /**
     * If some third-party app launched Chrome with an intent, and the URL got redirected, and the
     * user explicitly chose Chrome over other intent handlers, stay in Chrome unless there was a
     * new intent handler after redirection or Chrome cannot handle it any more.
     * Custom tabs are an exception to this rule, since at no point, the user sees an intent picker
     * and "picking Chrome" is handled inside the support library.
     */
    private boolean shouldKeepIntentRedirectInChrome(ExternalNavigationParams params,
            boolean incomingIntentRedirect, List<ResolveInfo> resolvingInfos,
            boolean isExternalProtocol) {
        if (params.getRedirectHandler() != null && incomingIntentRedirect && !isExternalProtocol
                && !params.getRedirectHandler().isFromCustomTabIntent()
                && !params.getRedirectHandler().hasNewResolver(resolvingInfos)) {
            if (DEBUG) Log.i(TAG, "Custom tab redirect no handled");
            return true;
        }
        return false;
    }

    /**
     * Returns whether the activity belongs to a WebAPK and the URL is within the scope of the
     * WebAPK. The WebAPK's main activity is a bouncer that redirects to WebApkActivity in Chrome.
     * In order to avoid bouncing indefinitely, we should not override the navigation if we are
     * currently showing the WebAPK (params#nativeClientPackageName()) that we will redirect to.
     */
    private boolean isAlreadyInTargetWebApk(
            List<ResolveInfo> resolveInfos, ExternalNavigationParams params) {
        String currentName = params.nativeClientPackageName();
        if (currentName == null) return false;
        for (ResolveInfo resolveInfo : resolveInfos) {
            ActivityInfo info = resolveInfo.activityInfo;
            if (info != null && currentName.equals(info.packageName)) {
                if (DEBUG) Log.i(TAG, "Already in WebAPK");
                return true;
            }
        }
        return false;
    }

    private boolean launchExternalIntent(Intent targetIntent, boolean shouldProxyForInstantApps) {
        try {
            if (!mDelegate.startActivityIfNeeded(targetIntent, shouldProxyForInstantApps)) {
                if (DEBUG) Log.i(TAG, "The current Activity was the only targeted Activity.");
                return false;
            }
        } catch (ActivityNotFoundException e) {
            // The targeted app must have been uninstalled/disabled since we queried for Activities
            // to handle this intent.
            if (DEBUG) Log.i(TAG, "Activity not found.");
            return false;
        }
        if (DEBUG) Log.i(TAG, "startActivityIfNeeded");
        return true;
    }

    private boolean handleWithAutofillAssistant(
            ExternalNavigationParams params, Intent targetIntent, String browserFallbackUrl) {
        if (mDelegate.handleWithAutofillAssistant(params, targetIntent, browserFallbackUrl)) {
            if (DEBUG) Log.i(TAG, "Handling with Assistant");
            return true;
        }
        return false;
    }

    private @OverrideUrlLoadingResult int shouldOverrideUrlLoadingInternal(
            ExternalNavigationParams params, Intent targetIntent,
            @Nullable String browserFallbackUrl) {
        sanitizeQueryIntentActivitiesIntent(targetIntent);

        if (blockExternalNavWhileBackgrounded(params) || blockExternalNavFromBackgroundTab(params)
                || ignoreBackForwardNav(params)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (handleWithAutofillAssistant(params, targetIntent, browserFallbackUrl)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(params.getUrl());

        if (isInternalPdfDownload(isExternalProtocol, params)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // This check should happen for reloads, navigations, etc..., which is why
        // it occurs before the subsequent blocks.
        if (startFileIntentIfNecessary(params, targetIntent)) {
            return OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION;
        }

        // This should come after file intents, but before any returns of
        // OVERRIDE_WITH_EXTERNAL_INTENT.
        if (externalIntentRequestsDisabledForUrl(params)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
        boolean isLink = pageTransitionCore == PageTransition.LINK;
        boolean isFormSubmit = pageTransitionCore == PageTransition.FORM_SUBMIT;
        boolean isFromIntent = (params.getPageTransition() & PageTransition.FROM_API) != 0;
        boolean linkNotFromIntent = isLink && !isFromIntent;

        boolean isOnEffectiveIntentRedirect = params.getRedirectHandler() == null
                ? false
                : params.getRedirectHandler().isOnEffectiveIntentRedirectChain();

        // http://crbug.com/170925: We need to show the intent picker when we receive an intent from
        // another app that 30x redirects to a YouTube/Google Maps/Play Store/Google+ URL etc.
        boolean incomingIntentRedirect =
                (isLink && isFromIntent && params.isRedirect()) || isOnEffectiveIntentRedirect;

        if (handleCCTRedirectsToInstantApps(params, isExternalProtocol, incomingIntentRedirect)) {
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        } else if (redirectShouldStayInChrome(params, isExternalProtocol, targetIntent)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (!preferToShowIntentPicker(params, pageTransitionCore, isExternalProtocol, isFormSubmit,
                    linkNotFromIntent, incomingIntentRedirect)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (isLinkFromChromeInternalPage(params)) return OverrideUrlLoadingResult.NO_OVERRIDE;

        if (handleWtaiMcProtocol(params)) {
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
        // TODO: handle other WTAI schemes.
        if (isUnhandledWtaiProtocol(params)) return OverrideUrlLoadingResult.NO_OVERRIDE;

        boolean hasIntentScheme = params.getUrl().startsWith(UrlConstants.INTENT_URL_SHORT_PREFIX)
                || params.getUrl().startsWith(UrlConstants.APP_INTENT_URL_SHORT_PREFIX);
        if (hasInternalScheme(params, targetIntent, hasIntentScheme)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (hasContentScheme(params, targetIntent, hasIntentScheme)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (isYoutubePairingCode(params)) return OverrideUrlLoadingResult.NO_OVERRIDE;

        if (shouldStayInIncognito(params, isExternalProtocol)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (!maybeSetSmsPackage(targetIntent)) maybeRecordPhoneIntentMetrics(targetIntent);

        if (hasIntentScheme) recordIntentActionMetrics(targetIntent);

        Intent debugIntent = new Intent(targetIntent);
        List<ResolveInfo> resolvingInfos = mDelegate.queryIntentActivities(targetIntent);
        if (resolvingInfos.isEmpty()) {
            return handleUnresolvableIntent(params, targetIntent, browserFallbackUrl);
        }

        if (browserFallbackUrl != null) targetIntent.removeExtra(EXTRA_BROWSER_FALLBACK_URL);

        boolean hasSpecializedHandler = mDelegate.countSpecializedHandlers(resolvingInfos) > 0;
        if (!isExternalProtocol && !hasSpecializedHandler) {
            if (fallBackToHandlingWithInstantApp(
                        params, incomingIntentRedirect, linkNotFromIntent)) {
                return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
            }
            return fallBackToHandlingInChrome();
        }

        // From this point on we should only have intents Chrome can't handle, or intents for apps
        // with specialized handlers.

        if (shouldStayWithinHost(
                    params, isLink, isFormSubmit, resolvingInfos, isExternalProtocol)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        boolean isDirectInstantAppsIntent =
                isExternalProtocol && mDelegate.isIntentToInstantApp(targetIntent);
        boolean shouldProxyForInstantApps = isDirectInstantAppsIntent && mDelegate.isSerpReferrer();
        if (preventDirectInstantAppsIntent(isDirectInstantAppsIntent, shouldProxyForInstantApps)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        prepareExternalIntent(targetIntent, params, resolvingInfos, shouldProxyForInstantApps);
        // As long as our intent resolution hasn't changed, resolvingInfos won't need to be
        // re-computed as it won't have changed.
        assert intentResolutionMatches(debugIntent, targetIntent);

        if (params.isIncognito() && !mDelegate.willChromeHandleIntent(targetIntent)) {
            return handleExternalIncognitoIntent(
                    targetIntent, params, browserFallbackUrl, shouldProxyForInstantApps);
        }

        if (shouldKeepIntentRedirectInChrome(
                    params, incomingIntentRedirect, resolvingInfos, isExternalProtocol)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (isAlreadyInTargetWebApk(resolvingInfos, params)) {
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        } else if (launchWebApkIfSoleIntentHandler(resolvingInfos, targetIntent)) {
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
        if (launchExternalIntent(targetIntent, shouldProxyForInstantApps)) {
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
        return OverrideUrlLoadingResult.NO_OVERRIDE;
    }

    /**
     * Sanitize intent to be passed to {@link ExternalNavigationDelegate#queryIntentActivities()}
     * ensuring that web pages cannot bypass browser security.
     */
    private void sanitizeQueryIntentActivitiesIntent(Intent intent) {
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }
    }

    /**
     * @return OVERRIDE_WITH_EXTERNAL_INTENT when we successfully started market activity,
     *         NO_OVERRIDE otherwise.
     */
    private @OverrideUrlLoadingResult int sendIntentToMarket(
            String packageName, String marketReferrer, ExternalNavigationParams params) {
        Uri marketUri =
                new Uri.Builder()
                        .scheme("market")
                        .authority("details")
                        .appendQueryParameter(PLAY_PACKAGE_PARAM, packageName)
                        .appendQueryParameter(PLAY_REFERRER_PARAM, Uri.decode(marketReferrer))
                        .build();
        Intent intent = new Intent(Intent.ACTION_VIEW, marketUri);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setPackage("com.android.vending");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (params.getReferrerUrl() != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(params.getReferrerUrl()));
        }

        if (!deviceCanHandleIntent(intent)) {
            // Exit early if the Play Store isn't available. (https://crbug.com/820709)
            if (DEBUG) Log.i(TAG, "Play Store not installed.");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (params.isIncognito()) {
            if (!mDelegate.startIncognitoIntent(intent, params.getReferrerUrl(), null,

                        params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(), false)) {
                if (DEBUG) Log.i(TAG, "Failed to show incognito alert dialog.");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }
            if (DEBUG) Log.i(TAG, "Incognito intent to Play Store.");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION;
        } else {
            mDelegate.startActivity(intent, false);
            if (DEBUG) Log.i(TAG, "Intent to Play Store.");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
    }

    /**
     * Clobber the current tab with fallback URL.
     *
     * @param browserFallbackUrl The fallback URL.
     * @param params The external navigation params.
     * @return {@link OverrideUrlLoadingResult} if the tab was clobbered, or we launched an
     *         intent.
     */
    private @OverrideUrlLoadingResult int clobberCurrentTabWithFallbackUrl(
            String browserFallbackUrl, ExternalNavigationParams params) {
        // If the fallback URL is a link to Play Store, send the user to Play Store app
        // instead: crbug.com/638672.
        Pair<String, String> appInfo = maybeGetPlayStoreAppIdAndReferrer(browserFallbackUrl);
        if (appInfo != null) {
            String marketReferrer = TextUtils.isEmpty(appInfo.second)
                    ? ContextUtils.getApplicationContext().getPackageName()
                    : appInfo.second;
            return sendIntentToMarket(appInfo.first, marketReferrer, params);
        }

        // For subframes, we don't support fallback url for now.
        // http://crbug.com/364522.
        if (!params.isMainFrame()) {
            if (DEBUG) Log.i(TAG, "Don't support fallback url in subframes");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // NOTE: any further redirection from fall-back URL should not override URL loading.
        // Otherwise, it can be used in chain for fingerprinting multiple app installation
        // status in one shot. In order to prevent this scenario, we notify redirection
        // handler that redirection from the current navigation should stay in Chrome.
        if (params.getRedirectHandler() != null) {
            params.getRedirectHandler().setShouldNotOverrideUrlLoadingOnCurrentRedirectChain();
        }
        if (DEBUG) Log.i(TAG, "clobberCurrentTab called");
        return mDelegate.clobberCurrentTab(browserFallbackUrl, params.getReferrerUrl());
    }

    /**
     * If the given URL is to Google Play, extracts the package name and referrer tracking code
     * from the {@param url} and returns as a Pair in that order. Otherwise returns null.
     */
    private Pair<String, String> maybeGetPlayStoreAppIdAndReferrer(String url) {
        Uri uri = Uri.parse(url);
        if (PLAY_HOSTNAME.equals(uri.getHost()) && uri.getPath() != null
                && uri.getPath().startsWith(PLAY_APP_PATH)
                && !TextUtils.isEmpty(uri.getQueryParameter(PLAY_PACKAGE_PARAM))) {
            return new Pair<String, String>(uri.getQueryParameter(PLAY_PACKAGE_PARAM),
                    uri.getQueryParameter(PLAY_REFERRER_PARAM));
        }
        return null;
    }

    /**
     * @return Whether the |url| could be handled by an external application on the system.
     */
    public boolean canExternalAppHandleUrl(String url) {
        if (url.startsWith(WTAI_MC_URL_PREFIX)) return true;
        Intent intent;
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ex) {
            // Ignore the error.
            Log.w(TAG, "Bad URI %s", url, ex);
            return false;
        }
        if (intent.getPackage() != null) return true;

        List<ResolveInfo> resolvingInfos = mDelegate.queryIntentActivities(intent);
        return resolvingInfos != null && !resolvingInfos.isEmpty();
    }

    /**
     * Dispatch SMS intents to the default SMS application if applicable.
     * Most SMS apps refuse to send SMS if not set as default SMS application.
     *
     * @param resolvingComponentNames The list of ComponentName that resolves the current intent.
     */
    private String getDefaultSmsPackageName(List<ResolveInfo> resolvingComponentNames) {
        String defaultSmsPackageName = mDelegate.getDefaultSmsPackageName();
        if (defaultSmsPackageName == null) return null;
        // Makes sure that the default SMS app actually resolves the intent.
        for (ResolveInfo resolveInfo : resolvingComponentNames) {
            if (defaultSmsPackageName.equals(resolveInfo.activityInfo.packageName)) {
                return defaultSmsPackageName;
            }
        }
        return null;
    }

    /**
     * Launches WebAPK if the WebAPK is the sole non-browser handler for the given intent.
     * @return Whether a WebAPK was launched.
     */
    private boolean launchWebApkIfSoleIntentHandler(
            List<ResolveInfo> resolvingInfos, Intent targetIntent) {
        ArrayList<String> packages = mDelegate.getSpecializedHandlers(resolvingInfos);
        if (packages.size() != 1 || !mDelegate.isValidWebApk(packages.get(0))) return false;
        Intent webApkIntent = new Intent(targetIntent);
        webApkIntent.setPackage(packages.get(0));
        try {
            mDelegate.startActivity(webApkIntent, false);
            if (DEBUG) Log.i(TAG, "Launched WebAPK");
            return true;
        } catch (ActivityNotFoundException e) {
            // The WebApk must have been uninstalled/disabled since we queried for Activities to
            // handle this intent.
            if (DEBUG) Log.i(TAG, "WebAPK launch failed");
            return false;
        }
    }

    /**
     * Returns whether or not there's an activity available to handle the intent.
     */
    private boolean deviceCanHandleIntent(Intent intent) {
        List<ResolveInfo> resolveInfos = mDelegate.queryIntentActivities(intent);
        return resolveInfos != null && !resolveInfos.isEmpty();
    }

    private static boolean intentResolutionMatches(Intent intent, Intent other) {
        return intent.filterEquals(other)
                && (intent.getSelector() == other.getSelector()
                        || intent.getSelector().filterEquals(other.getSelector()));
    }

    private void recordIntentActionMetrics(Intent intent) {
        String action = intent.getAction();
        @StandardActions
        int standardAction;
        if (TextUtils.isEmpty(action)) {
            standardAction = StandardActions.VIEW;
        } else {
            standardAction = getStandardAction(action);
        }
        RecordHistogram.recordEnumeratedHistogram(
                INTENT_ACTION_HISTOGRAM, standardAction, StandardActions.NUM_ENTRIES);
    }

    private @StandardActions int getStandardAction(String action) {
        switch (action) {
            case Intent.ACTION_MAIN:
                return StandardActions.MAIN;
            case Intent.ACTION_VIEW:
                return StandardActions.VIEW;
            case Intent.ACTION_ATTACH_DATA:
                return StandardActions.ATTACH_DATA;
            case Intent.ACTION_EDIT:
                return StandardActions.EDIT;
            case Intent.ACTION_PICK:
                return StandardActions.PICK;
            case Intent.ACTION_CHOOSER:
                return StandardActions.CHOOSER;
            case Intent.ACTION_GET_CONTENT:
                return StandardActions.GET_CONTENT;
            case Intent.ACTION_DIAL:
                return StandardActions.DIAL;
            case Intent.ACTION_CALL:
                return StandardActions.CALL;
            case Intent.ACTION_SEND:
                return StandardActions.SEND;
            case Intent.ACTION_SENDTO:
                return StandardActions.SENDTO;
            case Intent.ACTION_ANSWER:
                return StandardActions.ANSWER;
            case Intent.ACTION_INSERT:
                return StandardActions.INSERT;
            case Intent.ACTION_DELETE:
                return StandardActions.DELETE;
            case Intent.ACTION_RUN:
                return StandardActions.RUN;
            case Intent.ACTION_SYNC:
                return StandardActions.SYNC;
            case Intent.ACTION_PICK_ACTIVITY:
                return StandardActions.PICK_ACTIVITY;
            case Intent.ACTION_SEARCH:
                return StandardActions.SEARCH;
            case Intent.ACTION_WEB_SEARCH:
                return StandardActions.WEB_SEARCH;
            case Intent.ACTION_FACTORY_TEST:
                return StandardActions.FACTORY_TEST;
            default:
                return StandardActions.OTHER;
        }
    }
}

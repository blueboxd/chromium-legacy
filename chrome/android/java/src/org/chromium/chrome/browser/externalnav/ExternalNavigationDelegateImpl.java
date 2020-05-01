// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.Manifest.permission;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.StrictMode;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.WindowManager.BadTokenException;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.instantapps.AuthenticatedProxyActivity;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.util.ArrayList;
import java.util.List;

/**
 * The main implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    protected final Context mApplicationContext;
    private final Tab mTab;
    private final TabObserver mTabObserver;
    private boolean mIsTabDestroyed;

    public ExternalNavigationDelegateImpl(Tab tab) {
        mTab = tab;
        mApplicationContext = ContextUtils.getApplicationContext();
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onDestroyed(Tab tab) {
                mIsTabDestroyed = true;
            }
        };
        mTab.addObserver(mTabObserver);
    }

    /**
     * Get a {@link Context} linked to this delegate with preference to {@link Activity}.
     * The tab this delegate associates with can swap the {@link Activity} it is hosted in and
     * during the swap, there might not be an available {@link Activity}.
     * @return The activity {@link Context} if it can be reached.
     *         Application {@link Context} if not.
     */
    protected final Context getAvailableContext() {
        if (mTab.getWindowAndroid() == null) return mApplicationContext;
        Context activityContext =
                ContextUtils.activityFromContext(mTab.getWindowAndroid().getContext().get());
        if (activityContext == null) return mApplicationContext;
        return activityContext;
    }

    /**
     * Determines whether Chrome will be handling the given Intent.
     *
     * Note this function is slow on Android versions less than Lollipop.
     *
     * @param intent            Intent that will be fired.
     * @param matchDefaultOnly  See {@link PackageManager#MATCH_DEFAULT_ONLY}.
     * @return                  True if Chrome will definitely handle the intent, false otherwise.
     */
    public static boolean willChromeHandleIntent(Intent intent, boolean matchDefaultOnly) {
        Context context = ContextUtils.getApplicationContext();
        // Early-out if the intent targets Chrome.
        if (context.getPackageName().equals(intent.getPackage())
                || (intent.getComponent() != null
                        && context.getPackageName().equals(
                                intent.getComponent().getPackageName()))) {
            return true;
        }

        // Fall back to the more expensive querying of Android when the intent doesn't target
        // Chrome.
        ResolveInfo info = PackageManagerUtils.resolveActivity(
                intent, matchDefaultOnly ? PackageManager.MATCH_DEFAULT_ONLY : 0);
        return info != null && info.activityInfo.packageName.equals(context.getPackageName());
    }

    @Override
    public boolean willChromeHandleIntent(Intent intent) {
        return willChromeHandleIntent(intent, false);
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(String url) {
        return false;
    }

    @Override
    public int countSpecializedHandlers(List<ResolveInfo> infos) {
        return getSpecializedHandlersWithFilter(infos, null).size();
    }

    @Override
    public ArrayList<String> getSpecializedHandlers(List<ResolveInfo> infos) {
        return getSpecializedHandlersWithFilter(infos, null);
    }

    @VisibleForTesting
    public static ArrayList<String> getSpecializedHandlersWithFilter(
            List<ResolveInfo> infos, String filterPackageName) {
        ArrayList<String> result = new ArrayList<>();
        if (infos == null) {
            return result;
        }

        for (ResolveInfo info : infos) {
            if (!ExternalNavigationHandler.matchResolveInfoExceptWildCardHost(
                        info, filterPackageName)) {
                continue;
            }

            if (info.activityInfo != null) {
                if (InstantAppsHandler.getInstance().isInstantAppResolveInfo(info)) {
                    // Don't consider the Instant Apps resolver a specialized application.
                    continue;
                }

                result.add(info.activityInfo.packageName);
            } else {
                result.add("");
            }
        }
        return result;
    }

    /**
     * Check whether the given package is a specialized handler for the given intent
     *
     * @param packageName Package name to check against. Can be null or empty.
     * @param intent The intent to resolve for.
     * @return Whether the given package is a specialized handler for the given intent. If there is
     *         no package name given checks whether there is any specialized handler.
     */
    public static boolean isPackageSpecializedHandler(String packageName, Intent intent) {
        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER);
        return !getSpecializedHandlersWithFilter(handlers, packageName).isEmpty();
    }

    @Override
    public void startActivity(Intent intent, boolean proxy) {
        try {
            ExternalNavigationHandler.forcePdfViewerAsIntentHandlerIfNeeded(intent);
            if (proxy) {
                dispatchAuthenticatedIntent(intent);
            } else {
                Context context = getAvailableContext();
                if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            }
            ExternalNavigationHandler.recordExternalNavigationDispatched(intent);
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
        }
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent, boolean proxy) {
        boolean activityWasLaunched;
        // Only touches disk on Kitkat. See http://crbug.com/617725 for more context.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            ExternalNavigationHandler.forcePdfViewerAsIntentHandlerIfNeeded(intent);
            if (proxy) {
                dispatchAuthenticatedIntent(intent);
                activityWasLaunched = true;
            } else {
                Context context = getAvailableContext();
                if (context instanceof Activity) {
                    activityWasLaunched = ((Activity) context).startActivityIfNeeded(intent, -1);
                } else {
                    activityWasLaunched = false;
                }
            }
            if (activityWasLaunched) {
                ExternalNavigationHandler.recordExternalNavigationDispatched(intent);
            }
            return activityWasLaunched;
        } catch (SecurityException e) {
            // https://crbug.com/808494: Handle the URL in Chrome if dispatching to another
            // application fails with a SecurityException. This happens due to malformed manifests
            // in another app.
            return false;
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
            return false;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public boolean startIncognitoIntent(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        try {
            return startIncognitoIntentInternal(
                    intent, referrerUrl, fallbackUrl, needsToCloseTab, proxy);
        } catch (BadTokenException e) {
            return false;
        }
    }

    @Override
    public @OverrideUrlLoadingResult int handleIncognitoIntentTargetingSelf(
            final Intent intent, final String referrerUrl, final String fallbackUrl) {
        String primaryUrl = intent.getDataString();
        boolean isUrlLoadedInTheSameTab =
                loadUrlFromIntent(referrerUrl, primaryUrl, fallbackUrl, mTab, false, true);
        return (isUrlLoadedInTheSameTab) ? OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB
                                         : OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
    }

    private boolean startIncognitoIntentInternal(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        if (!hasValidTab()) return false;
        Context context = mTab.getWindowAndroid().getContext().get();
        if (!(context instanceof Activity)) return false;

        Activity activity = (Activity) context;
        new UiUtils.CompatibleAlertDialogBuilder(activity, R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.external_app_leave_incognito_warning_title)
                .setMessage(R.string.external_app_leave_incognito_warning)
                .setPositiveButton(R.string.external_app_leave_incognito_leave,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                try {
                                    startActivity(intent, proxy);
                                    if (mTab != null && !mTab.isClosing() && mTab.isInitialized()
                                            && needsToCloseTab) {
                                        closeTab();
                                    }
                                } catch (ActivityNotFoundException e) {
                                    // The activity that we thought was going to handle the intent
                                    // no longer exists, so catch the exception and assume Chrome
                                    // can handle it.
                                    loadUrlFromIntent(referrerUrl, fallbackUrl,
                                            intent.getDataString(), mTab, needsToCloseTab, true);
                                }
                            }
                        })
                .setNegativeButton(R.string.external_app_leave_incognito_stay,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                loadUrlFromIntent(referrerUrl, fallbackUrl, intent.getDataString(),
                                        mTab, needsToCloseTab, true);
                            }
                        })
                .setOnCancelListener(new OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        loadUrlFromIntent(referrerUrl, fallbackUrl, intent.getDataString(), mTab,
                                needsToCloseTab, true);
                    }
                })
                .show();
        return true;
    }

    @Override
    public void startFileIntent(
            final Intent intent, final String referrerUrl, final boolean needsToCloseTab) {
        PermissionCallback permissionCallback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED
                        && hasValidTab()) {
                    loadUrlFromIntent(referrerUrl, intent.getDataString(), null, mTab,
                            needsToCloseTab, mTab.isIncognito());
                } else {
                    // TODO(tedchoc): Show an indication to the user that the navigation failed
                    //                instead of silently dropping it on the floor.
                    if (needsToCloseTab) {
                        // If the access was not granted, then close the tab if necessary.
                        closeTab();
                    }
                }
            }
        };
        if (!hasValidTab()) return;
        mTab.getWindowAndroid().requestPermissions(
                new String[] {permission.READ_EXTERNAL_STORAGE}, permissionCallback);
    }

    private void loadUrlInNewTab(final String url, final boolean launchIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
        if (launchIncognito) intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setClassName(packageName, ChromeLauncherActivity.class.getName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.addTrustedIntentExtras(intent);
        startActivity(intent, false);
    }

    private void loadUrlInTab(Tab tab, final String url, final String referrerUrl) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        tab.loadUrl(loadUrlParams);
    }

    private boolean canLoadUrlInTab(Tab tab) {
        return !(tab == null || tab.isClosing() || !tab.isInitialized());
    }

    /**
     * Loads the URL from an intent, either in the current tab or a new tab, falling back to the
     * |alternateUrl| if the |primaryUrl| is unsupported.
     *
     * Handling is determined as follows:
     *
     * If the url scheme is not supported we do nothing.
     * If the url can be loaded in the |tab| then we load the url there.
     * If the url can't be loaded in the |tab| then we launch a new tab and load it there.
     *
     * @param referrerUrl The string containing the original url from where the intent was referred.
     * @param primaryUrl The primary url to load.
     * @param alternateUrl The fallback url to use if the primary url is null or invalid.
     * @param tab The current tab from where the navigation took place.
     * @param launchIncognito Whether the url should be loaded in an incognito tab.
     * @return true if the url is loaded in the |tab|.
     */
    private boolean loadUrlFromIntent(String referrerUrl, String primaryUrl, String alternateUrl,
            Tab tab, boolean needsToCloseTab, boolean launchIncognito) {
        // If we cannot load the URL in the provided tab, or we have an explicit request to close
        // the tab,  we need to open a new tab.
        boolean loadInNewTab = !canLoadUrlInTab(tab) || needsToCloseTab;

        boolean isPrimaryUrlValid =
                (primaryUrl != null) ? UrlUtilities.isAcceptedScheme(primaryUrl) : false;
        boolean isAlternateUrlValid =
                (alternateUrl != null) ? UrlUtilities.isAcceptedScheme(alternateUrl) : false;

        if (!isPrimaryUrlValid && !isAlternateUrlValid) return false;

        String url = (isPrimaryUrlValid) ? primaryUrl : alternateUrl;

        if (loadInNewTab) {
            loadUrlInNewTab(url, launchIncognito);
            // Explicit request to close the tab.
            if (needsToCloseTab) closeTab();
            return false;
        }

        loadUrlInTab(tab, url, referrerUrl);
        return true;
    }

    @Override
    public @OverrideUrlLoadingResult int clobberCurrentTab(String url, String referrerUrl) {
        int transitionType = PageTransition.LINK;
        final LoadUrlParams loadUrlParams = new LoadUrlParams(url, transitionType);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        if (hasValidTab()) {
            // Loading URL will start a new navigation which cancels the current one
            // that this clobbering is being done for. It leads to UAF. To avoid that,
            // we're loading URL asynchronously. See https://crbug.com/732260.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    // Tab might be closed when this is run. See https://crbug.com/662877
                    if (!mIsTabDestroyed) mTab.loadUrl(loadUrlParams);
                }
            });
            return OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB;
        } else {
            assert false : "clobberCurrentTab was called with an empty tab.";
            Uri uri = Uri.parse(url);
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            String packageName = ContextUtils.getApplicationContext().getPackageName();
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(packageName);
            startActivity(intent, false);
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
    }

    @Override
    public boolean isChromeAppInForeground() {
        return ApplicationStatus.getStateForApplication()
                == ApplicationState.HAS_RUNNING_ACTIVITIES;
    }

    @Override
    public void maybeSetWindowId(Intent intent) {
        Context context = getAvailableContext();
        if (!(context instanceof ChromeTabbedActivity2)) return;
        intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, 2);
    }

    private void closeTab() {
        if (!hasValidTab()) return;
        Context context = mTab.getWindowAndroid().getContext().get();
        if (context instanceof ChromeActivity) {
            ((ChromeActivity) context).getTabModelSelector().closeTab(mTab);
        }
    }

    @Override
    public void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp) {
        if (isIntentToInstantApp) {
            intent.putExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER, true);
        } else {
            // Make sure this extra is not sent unless we've done the verification.
            intent.removeExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER);
        }
    }

    @Override
    public void maybeSetUserGesture(Intent intent) {
        // The intent can be used to launch Chrome itself, record the user
        // gesture here so that it can be used later.
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent);
    }

    @Override
    public void maybeSetPendingReferrer(Intent intent, String referrerUrl) {
        IntentHandler.setPendingReferrer(intent, referrerUrl);
    }

    @Override
    public void maybeSetPendingIncognitoUrl(Intent intent) {
        IntentHandler.setPendingIncognitoUrl(intent);
    }

    @Override
    public boolean maybeLaunchInstantApp(
            String url, String referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer) {
        if (!hasValidTab() || mTab.getWebContents() == null) return false;

        InstantAppsHandler handler = InstantAppsHandler.getInstance();
        RedirectHandler redirect = RedirectHandlerTabHelper.getHandlerFor(mTab);
        Intent intent = redirect != null ? redirect.getInitialIntent() : null;
        // TODO(mariakhomenko): consider also handling NDEF_DISCOVER action redirects.
        if (isIncomingRedirect && intent != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
            // Set the URL the redirect was resolved to for checking the existence of the
            // instant app inside handleIncomingIntent().
            Intent resolvedIntent = new Intent(intent);
            resolvedIntent.setData(Uri.parse(url));
            return handler.handleIncomingIntent(getAvailableContext(), resolvedIntent,
                    LaunchIntentDispatcher.isCustomTabIntent(resolvedIntent), true);
        } else if (!isIncomingRedirect) {
            // Check if the navigation is coming from SERP and skip instant app handling.
            if (isSerpReferrer) return false;
            return handler.handleNavigation(getAvailableContext(), url,
                    TextUtils.isEmpty(referrerUrl) ? null : Uri.parse(referrerUrl), mTab);
        }
        return false;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        if (mTab == null) return null;
        return mTab.getWindowAndroid();
    }

    @Override
    public WebContents getWebContents() {
        if (mTab == null) return null;
        return mTab.getWebContents();
    }

    /**
     * Dispatches the intent through a proxy activity, so that startActivityForResult can be used
     * and the intent recipient can verify the caller.
     * @param intent The bare intent we were going to send.
     */
    protected void dispatchAuthenticatedIntent(Intent intent) {
        Intent proxyIntent = new Intent(Intent.ACTION_MAIN);
        proxyIntent.setClass(getAvailableContext(), AuthenticatedProxyActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        proxyIntent.putExtra(AuthenticatedProxyActivity.AUTHENTICATED_INTENT_EXTRA, intent);
        getAvailableContext().startActivity(proxyIntent);
    }

    /**
     * Starts the autofill assistant with the given intent. Exists to allow tests to stub out this
     * functionality.
     */
    protected void startAutofillAssistantWithIntent(
            Intent targetIntent, String browserFallbackUrl) {
        AutofillAssistantFacade.start(
                ((TabImpl) mTab).getActivity(), targetIntent.getExtras(), browserFallbackUrl);
    }

    /**
     * @return Whether or not we have a valid {@link Tab} available.
     */
    @Override
    public boolean hasValidTab() {
        return mTab != null && !mIsTabDestroyed;
    }

    @Override
    public boolean isIntentForTrustedCallingApp(Intent intent) {
        return false;
    }

    @Override
    public boolean isIntentToInstantApp(Intent intent) {
        return InstantAppsHandler.isIntentToInstantApp(intent);
    }

    @Override
    public boolean isIntentToAutofillAssistant(Intent intent) {
        return AutofillAssistantFacade.isAutofillAssistantByIntentTriggeringEnabled(intent);
    }

    @Override
    public boolean isValidWebApk(String packageName) {
        return WebApkValidator.isValidWebApk(ContextUtils.getApplicationContext(), packageName);
    }

    @Override
    public boolean handleWithAutofillAssistant(ExternalNavigationParams params, Intent targetIntent,
            String browserFallbackUrl, boolean isGoogleReferrer) {
        if (browserFallbackUrl != null && !params.isIncognito()
                && AutofillAssistantFacade.isAutofillAssistantByIntentTriggeringEnabled(
                        targetIntent)
                && isGoogleReferrer) {
            if (mTab != null) {
                startAutofillAssistantWithIntent(targetIntent, browserFallbackUrl);
            }
            return true;
        }
        return false;
    }
}

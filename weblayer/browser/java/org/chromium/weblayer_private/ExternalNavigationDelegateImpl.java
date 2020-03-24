// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.Manifest.permission;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.provider.Browser;
import android.provider.Telephony;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.PermissionCallback;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * WebLayer's implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    private static final String PDF_VIEWER = "com.google.android.apps.docs";
    private static final String PDF_MIME = "application/pdf";
    private static final String PDF_SUFFIX = ".pdf";
    private static final String PDF_EXTENSION = "pdf";

    protected final Context mApplicationContext;
    private final TabImpl mTab;
    private boolean mTabDestroyed;

    // TODO(crbug.com/1031465): Componentize IntentHandler's constant to dedupe this.
    private static final String ANDROID_APP_REFERRER_SCHEME = "android-app";
    // TODO(crbug.com/1031465): Componentize IntentHandler's constant to dedupe this.
    /**
     * Records package names of other applications in the system that could have handled
     * this intent.
     */
    public static final String EXTRA_EXTERNAL_NAV_PACKAGES = "org.chromium.chrome.browser.eenp";

    public ExternalNavigationDelegateImpl(TabImpl tab) {
        mTab = tab;
        mApplicationContext = ContextUtils.getApplicationContext();
    }

    public void onTabDestroyed() {
        mTabDestroyed = true;
    }

    /**
     * Get a {@link Context} linked to this delegate with preference to {@link Activity}.
     * The tab this delegate associates with can swap the {@link Activity} it is hosted in and
     * during the swap, there might not be an available {@link Activity}.
     * @return The activity {@link Context} if it can be reached.
     *         Application {@link Context} if not.
     */
    protected final Context getAvailableContext() {
        if (mTab.getBrowser().getContext() == null) return mApplicationContext;
        Context activityContext = ContextUtils.activityFromContext(mTab.getBrowser().getContext());
        if (activityContext == null) return mApplicationContext;
        return activityContext;
    }

    /**
     * If the intent is for a pdf, resolves intent handlers to find the platform pdf viewer if
     * it is available and force is for the provided |intent| so that the user doesn't need to
     * choose it from Intent picker.
     *
     * @param intent Intent to open.
     */
    public static void forcePdfViewerAsIntentHandlerIfNeeded(Intent intent) {
        if (intent == null || !isPdfIntent(intent)) return;
        resolveIntent(intent, true /* allowSelfOpen (ignored) */);
    }

    /**
     * Retrieve the best activity for the given intent. If a default activity is provided,
     * choose the default one. Otherwise, return the Intent picker if there are more than one
     * capable activities. If the intent is pdf type, return the platform pdf viewer if
     * it is available so user don't need to choose it from Intent picker.
     *
     * Note this function is slow on Android versions less than Lollipop.
     *
     * @param intent Intent to open.
     * @param allowSelfOpen Whether chrome itself is allowed to open the intent.
     * @return true if the intent can be resolved, or false otherwise.
     */
    public static boolean resolveIntent(Intent intent, boolean allowSelfOpen) {
        Context context = ContextUtils.getApplicationContext();
        ResolveInfo info = PackageManagerUtils.resolveActivity(intent, 0);
        if (info == null) return false;

        final String packageName = context.getPackageName();
        if (info.match != 0) {
            // There is a default activity for this intent, use that.
            return allowSelfOpen || !packageName.equals(info.activityInfo.packageName);
        }
        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.MATCH_DEFAULT_ONLY);
        if (handlers == null || handlers.isEmpty()) return false;
        boolean canSelfOpen = false;
        boolean hasPdfViewer = false;
        for (ResolveInfo resolveInfo : handlers) {
            String pName = resolveInfo.activityInfo.packageName;
            if (packageName.equals(pName)) {
                canSelfOpen = true;
            } else if (PDF_VIEWER.equals(pName)) {
                if (isPdfIntent(intent)) {
                    intent.setClassName(pName, resolveInfo.activityInfo.name);
                    // TODO(crbug.com/1031465): Use IntentHandler.java's version of this constant
                    // once it's componentized.
                    Uri referrer = new Uri.Builder()
                                           .scheme(ANDROID_APP_REFERRER_SCHEME)
                                           .authority(packageName)
                                           .build();
                    intent.putExtra(Intent.EXTRA_REFERRER, referrer);
                    hasPdfViewer = true;
                    break;
                }
            }
        }
        return !canSelfOpen || allowSelfOpen || hasPdfViewer;
    }

    private static boolean isPdfIntent(Intent intent) {
        if (intent == null || intent.getData() == null) return false;
        String filename = intent.getData().getLastPathSegment();
        return (filename != null && filename.endsWith(PDF_SUFFIX))
                || PDF_MIME.equals(intent.getType());
    }

    @Override
    public List<ResolveInfo> queryIntentActivities(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER);
    }

    @Override
    public boolean willChromeHandleIntent(Intent intent) {
        return false;
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
            if (!matchResolveInfoExceptWildCardHost(info, filterPackageName)) {
                continue;
            }

            if (info.activityInfo != null) {
                result.add(info.activityInfo.packageName);
            } else {
                result.add("");
            }
        }
        return result;
    }

    private static boolean matchResolveInfoExceptWildCardHost(
            ResolveInfo info, String filterPackageName) {
        IntentFilter intentFilter = info.filter;
        if (intentFilter == null) {
            // Error on the side of classifying ResolveInfo as generic.
            return false;
        }
        if (intentFilter.countDataAuthorities() == 0 && intentFilter.countDataPaths() == 0) {
            // Don't count generic handlers.
            return false;
        }
        boolean isWildCardHost = false;
        Iterator<IntentFilter.AuthorityEntry> it = intentFilter.authoritiesIterator();
        while (it != null && it.hasNext()) {
            IntentFilter.AuthorityEntry entry = it.next();
            if ("*".equals(entry.getHost())) {
                isWildCardHost = true;
                break;
            }
        }
        if (isWildCardHost) {
            return false;
        }
        if (!TextUtils.isEmpty(filterPackageName)
                && (info.activityInfo == null
                        || !info.activityInfo.packageName.equals(filterPackageName))) {
            return false;
        }
        return true;
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
        assert !proxy
            : "|proxy| should be true only for instant apps, which WebLayer doesn't handle";
        try {
            forcePdfViewerAsIntentHandlerIfNeeded(intent);
            Context context = getAvailableContext();
            if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
            recordExternalNavigationDispatched(intent);
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
        }
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent, boolean proxy) {
        assert !proxy
            : "|proxy| should be true only for instant apps, which WebLayer doesn't handle";

        boolean activityWasLaunched;
        // Only touches disk on Kitkat. See http://crbug.com/617725 for more context.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            forcePdfViewerAsIntentHandlerIfNeeded(intent);
            Context context = getAvailableContext();
            if (context instanceof Activity) {
                activityWasLaunched = ((Activity) context).startActivityIfNeeded(intent, -1);
            } else {
                activityWasLaunched = false;
            }
            if (activityWasLaunched) recordExternalNavigationDispatched(intent);
            return activityWasLaunched;
        } catch (SecurityException e) {
            // https://crbug.com/808494: Handle the URL in WebLayer if dispatching to another
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

    private void recordExternalNavigationDispatched(Intent intent) {
        ArrayList<String> specializedHandlers =
                intent.getStringArrayListExtra(EXTRA_EXTERNAL_NAV_PACKAGES);
        if (specializedHandlers != null && specializedHandlers.size() > 0) {
            RecordUserAction.record("MobileExternalNavigationDispatched");
        }
    }

    @Override
    public boolean startIncognitoIntent(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        // TODO(crbug.com/1063399): Determine if this behavior should be refined.
        startActivity(intent, proxy);
        return true;
    }

    @Override
    public boolean shouldRequestFileAccess(String url) {
        // If the tab is null, then do not attempt to prompt for access.
        if (!hasValidTab()) return false;

        // If the url points inside of Chromium's data directory, no permissions are necessary.
        // This is required to prevent permission prompt when uses wants to access offline pages.
        if (url.startsWith(UrlConstants.FILE_URL_PREFIX + PathUtils.getDataDirectory())) {
            return false;
        }

        return !mTab.getBrowser().getWindowAndroid().hasPermission(permission.READ_EXTERNAL_STORAGE)
                && mTab.getBrowser().getWindowAndroid().canRequestPermission(
                        permission.READ_EXTERNAL_STORAGE);
    }

    @Override
    public void startFileIntent(
            final Intent intent, final String referrerUrl, final boolean needsToCloseTab) {
        PermissionCallback permissionCallback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED
                        && hasValidTab()) {
                    String url = intent.getDataString();
                    LoadUrlParams loadUrlParams =
                            new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);
                    if (!TextUtils.isEmpty(referrerUrl)) {
                        Referrer referrer = new Referrer(referrerUrl, ReferrerPolicy.ALWAYS);
                        loadUrlParams.setReferrer(referrer);
                    }
                    mTab.loadUrl(loadUrlParams);
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
        mTab.getBrowser().getWindowAndroid().requestPermissions(
                new String[] {permission.READ_EXTERNAL_STORAGE}, permissionCallback);
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
                    if (hasValidTab()) mTab.loadUrl(loadUrlParams);
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
        return mTab.getBrowser().isResumed();
    }

    @Override
    public void maybeSetWindowId(Intent intent) {}

    @Override
    public String getDefaultSmsPackageName() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return null;
        return Telephony.Sms.getDefaultSmsPackage(mApplicationContext);
    }

    private void closeTab() {
        // Closing of tabs as part of intent launching is not yet implemented in WebLayer, and
        // parameters are specified such that this flow should never be invoked.
        // TODO(crbug.com/1031465): Adapt //chrome's logic for closing of tabs.
        assert false;
    }

    @Override
    public boolean isPdfDownload(String url) {
        String fileExtension = MimeTypeMap.getFileExtensionFromUrl(url);
        if (TextUtils.isEmpty(fileExtension)) return false;

        return PDF_EXTENSION.equals(fileExtension);
    }

    @Override
    public void maybeRecordAppHandlersInIntent(Intent intent, List<ResolveInfo> infos) {
        intent.putExtra(EXTRA_EXTERNAL_NAV_PACKAGES, getSpecializedHandlersWithFilter(infos, null));
    }

    @Override
    public void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetUserGesture(Intent intent) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingReferrer(Intent intent, String referrerUrl) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingIncognitoUrl(Intent intent) {}

    @Override
    public boolean isSerpReferrer() {
        // TODO (thildebr): Investigate whether or not we can use getLastCommittedUrl() instead of
        // the NavigationController.
        if (!hasValidTab() || mTab.getWebContents() == null) return false;

        NavigationController nController = mTab.getWebContents().getNavigationController();
        int index = nController.getLastCommittedEntryIndex();
        if (index == -1) return false;

        NavigationEntry entry = nController.getEntryAtIndex(index);
        if (entry == null) return false;

        return UrlUtilitiesJni.get().isGoogleSearchUrl(entry.getUrl());
    }

    @Override
    public boolean maybeLaunchInstantApp(
            String url, String referrerUrl, boolean isIncomingRedirect) {
        return false;
    }

    @Override
    public String getPreviousUrl() {
        if (mTab == null || mTab.getWebContents() == null) return null;
        return mTab.getWebContents().getLastCommittedUrl();
    }

    /**
     * @return Whether or not we have a valid {@link Tab} available.
     */
    private boolean hasValidTab() {
        assert mTab != null;
        return !mTabDestroyed;
    }

    @Override
    public boolean isIntentForTrustedCallingApp(Intent intent) {
        return false;
    }

    @Override
    public boolean isIntentToInstantApp(Intent intent) {
        return false;
    }

    @Override
    public boolean isValidWebApk(String packageName) {
        // TODO(crbug.com/1063874): Determine whether to refine this.
        return false;
    }

    @Override
    public boolean handleWithAutofillAssistant(
            ExternalNavigationParams params, Intent targetIntent, String browserFallbackUrl) {
        return false;
    }
}

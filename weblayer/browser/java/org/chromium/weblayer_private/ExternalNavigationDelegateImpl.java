// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.Manifest.permission;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.StrictMode;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.task.PostTask;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * WebLayer's implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    protected final Context mApplicationContext;
    private final TabImpl mTab;
    private boolean mTabDestroyed;

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
            if (!ExternalNavigationHandler.matchResolveInfoExceptWildCardHost(
                        info, filterPackageName)) {
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
            ExternalNavigationHandler.forcePdfViewerAsIntentHandlerIfNeeded(intent);
            Context context = getAvailableContext();
            if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
            ExternalNavigationHandler.recordExternalNavigationDispatched(intent);
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
            ExternalNavigationHandler.forcePdfViewerAsIntentHandlerIfNeeded(intent);
            Context context = getAvailableContext();
            if (context instanceof Activity) {
                activityWasLaunched = ((Activity) context).startActivityIfNeeded(intent, -1);
            } else {
                activityWasLaunched = false;
            }
            if (activityWasLaunched) {
                ExternalNavigationHandler.recordExternalNavigationDispatched(intent);
            }
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

    @Override
    public boolean startIncognitoIntent(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        // TODO(crbug.com/1063399): Determine if this behavior should be refined.
        startActivity(intent, proxy);
        return true;
    }

    // This method should never be invoked as WebLayer does not handle incoming intents.
    @Override
    public @OverrideUrlLoadingResult int handleIncognitoIntentTargetingSelf(
            final Intent intent, final String referrerUrl, final String fallbackUrl) {
        assert false;
        return OverrideUrlLoadingResult.NO_OVERRIDE;
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

    private void closeTab() {
        InterceptNavigationDelegateClientImpl.closeTab(mTab);
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
    public boolean maybeLaunchInstantApp(
            String url, String referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer) {
        return false;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        if (mTab == null) return null;
        return mTab.getBrowser().getWindowAndroid();
    }

    @Override
    public WebContents getWebContents() {
        if (mTab == null) return null;
        return mTab.getWebContents();
    }

    @Override
    public boolean hasValidTab() {
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
    public boolean isIntentToAutofillAssistant(Intent intent) {
        return false;
    }

    @Override
    public boolean isValidWebApk(String packageName) {
        // TODO(crbug.com/1063874): Determine whether to refine this.
        return false;
    }

    @Override
    public boolean handleWithAutofillAssistant(ExternalNavigationParams params, Intent targetIntent,
            String browserFallbackUrl, boolean isGoogleReferrer) {
        return false;
    }
}

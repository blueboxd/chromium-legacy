// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.fullscreen.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;

import javax.inject.Inject;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} owned
 * by a {@link CustomTabActivity}.
 */
@ActivityScope
public class CustomTabDelegateFactory implements TabDelegateFactory {
    /**
     * A custom external navigation delegate that forbids the intent picker from showing up.
     */
    static class CustomTabNavigationDelegate extends ExternalNavigationDelegateImpl {
        private static final String TAG = "customtabs";
        private final String mClientPackageName;
        private final ExternalAuthUtils mExternalAuthUtils;
        private boolean mHasActivityStarted;

        /**
         * Constructs a new instance of {@link CustomTabNavigationDelegate}.
         */
        CustomTabNavigationDelegate(Tab tab, String clientPackageName,
                ExternalAuthUtils authUtils) {
            super(tab);
            mClientPackageName = clientPackageName;
            mExternalAuthUtils = authUtils;
        }

        @Override
        public void startActivity(Intent intent, boolean proxy) {
            super.startActivity(intent, proxy);
            mHasActivityStarted = true;
        }

        @Override
        public boolean startActivityIfNeeded(Intent intent, boolean proxy) {
            boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(intent.toUri(0));
            boolean hasDefaultHandler = hasDefaultHandler(intent);
            try {
                // For a URL chrome can handle and there is no default set, handle it ourselves.
                if (!hasDefaultHandler) {
                    if (!TextUtils.isEmpty(mClientPackageName)
                            && isPackageSpecializedHandler(mClientPackageName, intent)) {
                        intent.setPackage(mClientPackageName);
                    } else if (!isExternalProtocol) {
                        return false;
                    }
                }

                if (proxy) {
                    dispatchAuthenticatedIntent(intent);
                    mHasActivityStarted = true;
                    return true;
                } else {
                    // If android fails to find a handler, handle it ourselves.
                    Context context = getAvailableContext();
                    if (context instanceof Activity
                            && ((Activity) context).startActivityIfNeeded(intent, -1)) {
                        mHasActivityStarted = true;
                        return true;
                    }
                }
                return false;
            } catch (SecurityException e) {
                // https://crbug.com/808494: Handle the URL in Chrome if dispatching to another
                // application fails with a SecurityException. This happens due to malformed
                // manifests in another app.
                return false;
            } catch (RuntimeException e) {
                IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
                return false;
            }
        }

        /**
         * Resolve the default external handler of an intent.
         * @return Whether the default external handler is found: if chrome turns out to be the
         *         default handler, this method will return false.
         */
        private boolean hasDefaultHandler(Intent intent) {
            try {
                ResolveInfo info =
                        mApplicationContext.getPackageManager().resolveActivity(intent, 0);
                if (info != null) {
                    final String chromePackage = mApplicationContext.getPackageName();
                    // If a default handler is found and it is not chrome itself, fire the intent.
                    if (info.match != 0 && !chromePackage.equals(info.activityInfo.packageName)) {
                        return true;
                    }
                }
            } catch (RuntimeException e) {
                IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
            }
            return false;
        }

        @Override
        public boolean isIntentForTrustedCallingApp(Intent intent) {
            if (TextUtils.isEmpty(mClientPackageName)) return false;
            if (!mExternalAuthUtils.isGoogleSigned(mClientPackageName)) return false;

            return isPackageSpecializedHandler(mClientPackageName, intent);
        }

        /**
         * @return Whether an external activity has started to handle a url. For testing only.
         */
        @VisibleForTesting
        public boolean hasExternalActivityStarted() {
            return mHasActivityStarted;
        }
    }

    private static class CustomTabWebContentsDelegate
            extends ActivityTabWebContentsDelegateAndroid {
        private final MultiWindowUtils mMultiWindowUtils;
        private final boolean mShouldEnableEmbeddedMediaExperience;

        /**
         * See {@link TabWebContentsDelegateAndroid}.
         */
        public CustomTabWebContentsDelegate(Tab tab, ChromeActivity activity,
                MultiWindowUtils multiWindowUtils, boolean shouldEnableEmbeddedMediaExperience) {
            super(tab, activity);
            mMultiWindowUtils = multiWindowUtils;
            mShouldEnableEmbeddedMediaExperience = shouldEnableEmbeddedMediaExperience;
        }

        @Override
        public boolean shouldResumeRequestsForCreatedWindow() {
            return true;
        }

        @Override
        protected void bringActivityToForeground() {
            // No-op here. If client's task is in background Chrome is unable to foreground it.
        }

        @Override
        protected boolean shouldEnableEmbeddedMediaExperience() {
            return mShouldEnableEmbeddedMediaExperience;
        }

        @Override
        public void openNewTab(String url, String extraHeaders, ResourceRequestBody postData,
                int disposition, boolean isRendererInitiated) {
            // If attempting to open an incognito tab, always send the user to tabbed mode.
            if (disposition == WindowOpenDisposition.OFF_THE_RECORD) {
                if (isRendererInitiated) {
                    throw new IllegalStateException(
                            "Invalid attempt to open an incognito tab from the renderer");
                }
                LoadUrlParams loadUrlParams = new LoadUrlParams(url);
                loadUrlParams.setVerbatimHeaders(extraHeaders);
                loadUrlParams.setPostData(postData);
                loadUrlParams.setIsRendererInitiated(isRendererInitiated);

                Class<? extends ChromeTabbedActivity> tabbedClass =
                        mMultiWindowUtils.getTabbedActivityForIntent(
                                null, ContextUtils.getApplicationContext());
                AsyncTabCreationParams tabParams = new AsyncTabCreationParams(loadUrlParams,
                        new ComponentName(ContextUtils.getApplicationContext(), tabbedClass));
                new TabDelegate(true).createNewTab(tabParams,
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND, TabModel.INVALID_TAB_INDEX);
                return;
            }

            super.openNewTab(url, extraHeaders, postData, disposition, isRendererInitiated);
        }
    }

    private final ChromeActivity mActivity;
    private final boolean mShouldHideBrowserControls;
    private final boolean mIsOpenedByChrome;
    private final boolean mShouldAllowAppBanners;
    private final boolean mShouldEnableEmbeddedMediaExperience;
    private final BrowserControlsVisibilityDelegate mBrowserStateVisibilityDelegate;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final MultiWindowUtils mMultiWindowUtils;

    private ExternalNavigationDelegateImpl mNavigationDelegate;

    /**
     * @param activity {@link ChromeActivity} instance.
     * @param shouldHideBrowserControls Whether or not the browser controls may auto-hide.
     * @param isOpenedByChrome Whether the CustomTab was originally opened by Chrome.
     * @param shouldAllowAppBanners Whether app install banners can be shown.
     * @param shouldEnableEmbeddedMediaExperience Whether embedded media experience is enabled.
     * @param visibilityDelegate The delegate that handles browser control visibility associated
     *                           with browser actions (as opposed to tab state).
     */
    private CustomTabDelegateFactory(ChromeActivity activity, boolean shouldHideBrowserControls,
            boolean isOpenedByChrome, boolean shouldAllowAppBanners,
            boolean shouldEnableEmbeddedMediaExperience,
            BrowserControlsVisibilityDelegate visibilityDelegate, ExternalAuthUtils authUtils,
            MultiWindowUtils multiWindowUtils) {
        mActivity = activity;
        mShouldHideBrowserControls = shouldHideBrowserControls;
        mIsOpenedByChrome = isOpenedByChrome;
        mShouldAllowAppBanners = shouldAllowAppBanners;
        mShouldEnableEmbeddedMediaExperience = shouldEnableEmbeddedMediaExperience;
        mBrowserStateVisibilityDelegate = visibilityDelegate;
        mExternalAuthUtils = authUtils;
        mMultiWindowUtils = multiWindowUtils;
    }

    @Inject
    public CustomTabDelegateFactory(ChromeActivity activity,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabBrowserControlsVisibilityDelegate visibilityDelegate,
            ExternalAuthUtils authUtils, MultiWindowUtils multiWindowUtils) {
        // Don't show an app install banner for the user of a Trusted Web Activity - they've already
        // got an app installed!
        this(activity, intentDataProvider.shouldEnableUrlBarHiding(),
                intentDataProvider.isOpenedByChrome(), !intentDataProvider.isTrustedWebActivity(),
                intentDataProvider.shouldEnableEmbeddedMediaExperience(), visibilityDelegate,
                authUtils, multiWindowUtils);
    }

    /**
     * Creates a basic/empty CustomTabDelegateFactory for use when creating a hidden tab. It will
     * be replaced when the hidden Tab becomes shown.
     */
    static CustomTabDelegateFactory createDummy() {
        return new CustomTabDelegateFactory(null, false, false, false, false, null, null, null);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        TabStateBrowserControlsVisibilityDelegate tabDelegate =
                new TabStateBrowserControlsVisibilityDelegate(tab) {
                    @Override
                    public boolean canAutoHideBrowserControls() {
                        return mShouldHideBrowserControls && super.canAutoHideBrowserControls();
                    }
                };

        // mBrowserStateVisibilityDelegate == null for background tabs for which
        // fullscreen state info from BrowserStateVisibilityDelegate is not available.
        return mBrowserStateVisibilityDelegate == null
                ? tabDelegate
                : new ComposedBrowserControlsVisibilityDelegate(
                        tabDelegate, mBrowserStateVisibilityDelegate);
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new CustomTabWebContentsDelegate(
                tab, mActivity, mMultiWindowUtils, mShouldEnableEmbeddedMediaExperience);
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        if (mIsOpenedByChrome) {
            mNavigationDelegate = new ExternalNavigationDelegateImpl(tab);
        } else {
            mNavigationDelegate = new CustomTabNavigationDelegate(
                    tab, TabAssociatedApp.getAppId(tab), mExternalAuthUtils);
        }
        return new ExternalNavigationHandler(mNavigationDelegate);
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(Tab tab) {
        return new ChromeContextMenuPopulator(new TabContextMenuItemDelegate(tab),
                ChromeContextMenuPopulator.ContextMenuMode.CUSTOM_TAB);
    }

    @Override
    public boolean canShowAppBanners() {
        return mShouldAllowAppBanners;
    }

    /**
     * @return The {@link CustomTabNavigationDelegate} in this tab. For test purpose only.
     */
    @VisibleForTesting
    ExternalNavigationDelegateImpl getExternalNavigationDelegate() {
        return mNavigationDelegate;
    }
}

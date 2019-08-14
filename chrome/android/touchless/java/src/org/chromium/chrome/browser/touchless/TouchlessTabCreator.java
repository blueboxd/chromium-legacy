// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.touchless;

import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.LaunchSourceType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelOrderController;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Creates Tabs for navigation originating from {@link NoTouchActivity}.
 *
 * This is the same as the parent class with exception of opening a Custom Tab instead of creating a
 * new tab in Chrome for links that open in new tabs.
 */
public class TouchlessTabCreator extends ChromeTabCreator {
    private final TabDelegate mAsyncTabDelegate;
    private final TabModelOrderController mOrderController;

    public TouchlessTabCreator(NoTouchActivity activity, WindowAndroid window, boolean incognito) {
        super(activity, window, incognito);
        mAsyncTabDelegate = new TabDelegate(incognito) {
            @Override
            public void createNewTab(
                    AsyncTabCreationParams asyncParams, @TabLaunchType int type, int parentId) {
                String url = asyncParams.getLoadUrlParams().getUrl();

                int assignedTabId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
                AsyncTabParamsManager.add(assignedTabId, asyncParams);

                Intent intent = new CustomTabsIntent.Builder().setShowTitle(true).build().intent;
                intent.setData(Uri.parse(url));
                intent.putExtra(
                        CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, true);
                intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME, true);
                intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_WEBAPK, false);
                intent.putExtra(CustomTabIntentDataProvider.EXTRA_BROWSER_LAUNCH_SOURCE,
                        LaunchSourceType.OTHER);
                intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                        ContextUtils.getApplicationContext().getPackageName());
                addAsyncTabExtras(
                        asyncParams, parentId, false /* isChromeUI */, assignedTabId, intent);

                IntentHandler.startActivityForTrustedIntent(intent);
            }
        };

        mOrderController = new TabModelOrderController() {
            @Override
            public int determineInsertionIndex(int type, int position, Tab newTab) {
                return 0;
            }

            @Override
            public int determineInsertionIndex(int type, Tab newTab) {
                return 0;
            }

            @Override
            public boolean willOpenInForeground(int type, boolean isNewTabIncognito) {
                return true;
            }
        };
    }

    @Override
    public boolean createTabWithWebContents(
            Tab parent, WebContents webContents, @TabLaunchType int type, String url) {
        if (shouldRedirectToCCT(type, url)) {
            return mAsyncTabDelegate.createTabWithWebContents(parent, webContents, type, url);
        }
        return super.createTabWithWebContents(parent, webContents, type, url);
    }

    @Override
    public Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, Intent intent) {
        if (shouldRedirectToCCT(type, loadUrlParams.getUrl())) {
            return mAsyncTabDelegate.createNewTab(loadUrlParams, type, parent);
        }
        return super.createNewTab(loadUrlParams, type, parent, intent);
    }

    private boolean shouldRedirectToCCT(@TabLaunchType int type, String url) {
        // Only link clicks should open in CCT, if the browser opens a tab for, say, intent
        // handling, we should continue to open in NoTouchActivity.
        // However, we should also handle intent URLs as normal, even if they open in a new window.
        return isLinkClickLaunchType(type) && !UrlUtilities.validateIntentUrl(url);
    }

    private boolean isLinkClickLaunchType(@TabLaunchType int type) {
        return type == TabLaunchType.FROM_LINK || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND;
    }

    /* package */ void setTabModel(TabModel model) {
        setTabModel(model, mOrderController);
    }
}

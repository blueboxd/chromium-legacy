// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.touchless;

import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.LaunchSourceType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.UrlUtilities;

import androidx.browser.customtabs.CustomTabsIntent;

/**
 * Asynchronously creates Tabs for navigation originating from {@link NoTouchActivity}.
 *
 * This is the same as the parent class with exception of opening a Custom Tab instead of creating a
 * new tab in Chrome.
 */
public class TouchlessTabDelegate extends TabDelegate {
    public TouchlessTabDelegate(boolean incognito) {
        super(incognito);
    }

    @Override
    public void createNewTab(
            AsyncTabCreationParams asyncParams, @TabLaunchType int type, int parentId) {
        if (!isLinkClickLaunchType(type)) {
            // Only link clicks should open in CCT, if the browser opens a tab for, say, intent
            // handling, we should continue to open in NoTouchActivity.
            super.createNewTab(asyncParams, type, parentId);
            return;
        }
        if (UrlUtilities.validateIntentUrl(asyncParams.getLoadUrlParams().getUrl())) {
            // Handle intent URLs as normal.
            super.createNewTab(asyncParams, type, parentId);
            return;
        }

        String url = asyncParams.getLoadUrlParams().getUrl();

        int assignedTabId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
        AsyncTabParamsManager.add(assignedTabId, asyncParams);

        Intent intent = new CustomTabsIntent.Builder().setShowTitle(true).build().intent;
        intent.setData(Uri.parse(url));
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_WEBAPK, false);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_BROWSER_LAUNCH_SOURCE, LaunchSourceType.OTHER);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                ContextUtils.getApplicationContext().getPackageName());
        addAsyncTabExtras(asyncParams, parentId, false /* isChromeUI */, assignedTabId, intent);

        IntentHandler.startActivityForTrustedIntent(intent);
    }

    private boolean isLinkClickLaunchType(@TabLaunchType int type) {
        return type == TabLaunchType.FROM_LINK || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND;
    }
}

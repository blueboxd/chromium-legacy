// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareActivity;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * A simple activity that shows sharing QR code option in share menu.
 */
public class QrCodeShareActivity extends ShareActivity {
    private ActivityTabProvider mActivityTabProvider;
    private TabCreatorManager.TabCreator mTabCreator;

    @Override
    protected void handleShareAction(ChromeActivity triggeringActivity) {
        mActivityTabProvider = triggeringActivity.getActivityTabProvider();
        mTabCreator = triggeringActivity.getCurrentTabCreator();

        QrCodeCoordinator qrCodeCoordinator =
                new QrCodeCoordinator(triggeringActivity, this::createNewTab);
        qrCodeCoordinator.show();
    }

    public static boolean featureIsAvailable() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SHARING_QR_CODE_ANDROID);
    }

    private void createNewTab(String url) {
        mTabCreator.createNewTab(
                new LoadUrlParams(url), TabLaunchType.FROM_LINK, mActivityTabProvider.get());
    }
}
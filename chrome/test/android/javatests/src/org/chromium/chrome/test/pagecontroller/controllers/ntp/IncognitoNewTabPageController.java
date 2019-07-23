// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Incognito NTP Page Controller.
 */
public class IncognitoNewTabPageController extends PageController {
    private static final IUi2Locator LOCATOR_INCOGNITO_PAGE =
            Ui2Locators.withResEntries(R.id.new_tab_incognito_container);

    private static final IncognitoNewTabPageController sInstance =
            new IncognitoNewTabPageController();
    private IncognitoNewTabPageController() {}
    public static IncognitoNewTabPageController getInstance() {
        return sInstance;
    }

    @Override
    public boolean isCurrentPageThis() {
        return mLocatorHelper.isOnScreen(LOCATOR_INCOGNITO_PAGE);
    }
}

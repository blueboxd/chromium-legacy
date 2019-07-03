// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * New Tab Page Page Controller, handles either Feed or Zine implementations.
 */
public class NewTabPageController extends PageController {
    private static final IUi2Locator LOCATOR_NEW_TAB_PAGE =
            Ui2Locators.withResIds("ntp_content", "feed_stream_recycler_view", "card_contents");

    private static NewTabPageController sInstance = new NewTabPageController();
    private NewTabPageController() {}
    public static NewTabPageController getInstance() {
        return sInstance;
    }

    @Override
    public boolean isCurrentPageThis() {
        return mLocatorHelper.isOnScreen(LOCATOR_NEW_TAB_PAGE);
    }
}

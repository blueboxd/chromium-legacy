// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.first_run;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * TOS Dialog (part of the First Run Experience) Page Controller.
 */
public class TOSController extends PageController {
    private static final IUi2Locator LOCATOR_TOS = Ui2Locators.withResEntries(R.id.tos_and_privacy);
    private static final IUi2Locator LOCATOR_SEND_REPORT_CHECKBOX =
            Ui2Locators.withResEntries(R.id.send_report_checkbox);
    private static final IUi2Locator LOCATOR_ACCEPT = Ui2Locators.withResEntries(R.id.terms_accept);

    private static final TOSController sInstance = new TOSController();
    private TOSController() {}
    public static TOSController getInstance() {
        return sInstance;
    }

    @Override
    public boolean isCurrentPageThis() {
        return mLocatorHelper.isOnScreen(LOCATOR_TOS);
    }

    public boolean isSendingReports() {
        return mLocatorHelper.getOneChecked(LOCATOR_SEND_REPORT_CHECKBOX);
    }

    public TOSController toggleSendReports() {
        mUtils.click(LOCATOR_SEND_REPORT_CHECKBOX, LOCATOR_TOS);
        return this;
    }

    public void acceptAndContinue() {
        mUtils.click(LOCATOR_ACCEPT);
    }
}

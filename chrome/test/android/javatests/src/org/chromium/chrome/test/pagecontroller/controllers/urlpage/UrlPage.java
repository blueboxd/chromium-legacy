// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.urlpage;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.tabswitcher.TabSwitcherController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

/**
 * Url Page (showing a loaded URL) Page Controller.
 */
// TODO(aluo): merge incognito page into this.
public class UrlPage extends PageController {
    private static final long PAGE_LOAD_TIMEOUT = 10000L;
    private static final IUi2Locator LOCATOR_WEB_VIEW =
            Ui2Locators.withPath(Ui2Locators.withResEntries(R.id.content),
                    Ui2Locators.withClassRegex("android\\.webkit\\.WebView"));
    private static final IUi2Locator LOCATOR_URL_BAR = Ui2Locators.withResEntries(R.id.url_bar);
    private static final IUi2Locator LOCATOR_TAB_SWITCHER =
            Ui2Locators.withResEntries(R.id.tab_switcher_button);
    private static final IUi2Locator LOCATOR_MENU = Ui2Locators.withResEntries(R.id.menu_button);

    private static final UrlPage sInstance = new UrlPage();
    private UrlPage() {}
    public static UrlPage getInstance() {
        return sInstance;
    }

    @Override
    public boolean isCurrentPageThis() {
        long savedTimeout = mUtils.getTimeout();
        UiLocatorHelper helper = mUtils.getLocatorHelper(PAGE_LOAD_TIMEOUT);
        return helper.isOnScreen(LOCATOR_WEB_VIEW);
    }

    public String getUrl() {
        return mLocatorHelper.getOneText(LOCATOR_URL_BAR);
    }

    public boolean isTextFoundAtCurrentScroll(String text) {
        IUi2Locator locator =
                Ui2Locators.withPath(LOCATOR_WEB_VIEW, Ui2Locators.withTextContaining(text));
        return mLocatorHelper.isOnScreen(locator);
    }

    public TabSwitcherController openTabSwitcher() {
        mUtils.click(LOCATOR_TAB_SWITCHER);
        TabSwitcherController inst = TabSwitcherController.getInstance();
        inst.verify();
        return inst;
    }
}

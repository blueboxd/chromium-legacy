// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import com.google.android.libraries.feed.sharedstream.contextmenumanager.R;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.urlpage.UrlPage;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Article Actions Menu (long-press on NTP article) Page Controller.
 */
public class ArticleActionsMenu extends PageController {
    private static final IUi2Locator LOCATOR_MENU = Ui2Locators.withClassRegex(".*ListView");

    private static final IUi2Locator LOCATOR_OPEN_NEW_TAB = Ui2Locators.withResEntriesByIndex(
            0, org.chromium.chrome.R.id.title, R.id.feed_simple_list_item);
    private static final IUi2Locator LOCATOR_OPEN_INCOGNITO = Ui2Locators.withResEntriesByIndex(
            1, org.chromium.chrome.R.id.title, R.id.feed_simple_list_item);
    private static final IUi2Locator LOCATOR_DOWNLOAD_LINK = Ui2Locators.withResEntriesByIndex(
            2, org.chromium.chrome.R.id.title, R.id.feed_simple_list_item);
    private static final IUi2Locator LOCATOR_REMOVE = Ui2Locators.withResEntriesByIndex(
            3, org.chromium.chrome.R.id.title, R.id.feed_simple_list_item);
    private static final IUi2Locator LOCATOR_LEARN_MORE = Ui2Locators.withResEntriesByIndex(
            4, org.chromium.chrome.R.id.title, R.id.feed_simple_list_item);

    static private ArticleActionsMenu sInstance = new ArticleActionsMenu();
    private ArticleActionsMenu() {}
    static public ArticleActionsMenu getInstance() {
        return sInstance;
    }

    @Override
    public boolean isCurrentPageThis() {
        return mLocatorHelper.isOnScreen(LOCATOR_LEARN_MORE);
    }

    public UrlPage clickOpenNewTab() {
        mUtils.click(LOCATOR_OPEN_NEW_TAB);
        UrlPage inst = UrlPage.getInstance();
        inst.verify();
        return inst;
    }

    public UrlPage clickOpenIncognitoTab() {
        mUtils.click(LOCATOR_OPEN_INCOGNITO);
        UrlPage inst = UrlPage.getInstance();
        inst.verify();
        return inst;
    }

    public void clickDownloadLink() {
        mUtils.click(LOCATOR_DOWNLOAD_LINK);
    }

    public NewTabPageController clickRemoveArticle() {
        mUtils.click(LOCATOR_REMOVE);
        NewTabPageController inst = NewTabPageController.getInstance();
        inst.verify();
        return inst;
    }

    public void clickLearnMore() {
        mUtils.click(LOCATOR_LEARN_MORE);
    }

    public NewTabPageController dismiss() {
        mUtils.clickOutsideOf(LOCATOR_MENU);
        NewTabPageController inst = NewTabPageController.getInstance();
        inst.verify();
        return inst;
    }
}

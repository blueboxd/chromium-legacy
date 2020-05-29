// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

/**
 * Class for controlling the page info cookies section.
 */
public class PageInfoCookiesController implements PageInfoSubpageController {
    private PageInfoController mMainController;
    private PageInfoViewV2 mView;
    private String mTitle;

    public PageInfoCookiesController(
            PageInfoController mainController, PageInfoViewV2 view, boolean isVisible) {
        mMainController = mainController;
        mView = view;
        mTitle = mView.getContext().getResources().getString(R.string.cookies_title);
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = isVisible;
        rowParams.title = mTitle;
        rowParams.clickCallback = this::launchSubpage;
        mView.getCookiesRowView().setParams(rowParams);
    }

    private void launchSubpage() {
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mTitle;
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        // TODO(crbug.com/1077766): Create and set the cookie specific view.
        return null;
    }

    @Override
    public void willRemoveSubpage() {}

    public void onBlockedCookiesCountChanged(int blockedCookies) {
        String subtitle = mView.getContext().getResources().getQuantityString(
                R.plurals.cookie_controls_blocked_cookies, blockedCookies, blockedCookies);
        mView.getCookiesRowView().updateSubtitle(subtitle);
    }
}

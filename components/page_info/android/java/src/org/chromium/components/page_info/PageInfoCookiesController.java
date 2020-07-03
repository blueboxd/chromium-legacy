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
    private PageInfoMainPageController mMainController;
    private PageInfoRowView mRowView;
    private String mFullUrl;
    private String mTitle;

    public PageInfoCookiesController(PageInfoMainPageController mainController,
            PageInfoRowView rowView, boolean isVisible, String fullUrl) {
        mMainController = mainController;
        mRowView = rowView;
        mFullUrl = fullUrl;
        mTitle = mRowView.getContext().getResources().getString(R.string.cookies_title);

        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = isVisible;
        rowParams.title = mTitle;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
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
    public void onSubPageAttached() {}

    @Override
    public void onSubpageRemoved() {}

    public void onBlockedCookiesCountChanged(int blockedCookies) {
        String subtitle = mRowView.getContext().getResources().getQuantityString(
                R.plurals.cookie_controls_blocked_cookies, blockedCookies, blockedCookies);
        mRowView.updateSubtitle(subtitle);
    }
}

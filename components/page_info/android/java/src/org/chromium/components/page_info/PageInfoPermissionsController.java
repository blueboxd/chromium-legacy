// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

/**
 * Class for controlling the page info permissions section.
 */
public class PageInfoPermissionsController implements PageInfoSubpageController {
    private PageInfoMainPageController mMainController;
    private PageInfoRowView mRowView;
    private String mTitle;

    public PageInfoPermissionsController(
            PageInfoMainPageController mainController, PageInfoRowView view) {
        mMainController = mainController;
        mRowView = view;
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
        // TODO(crbug.com/1077766): Create and set the permissions specific view.
        return null;
    }

    @Override
    public void onSubPageAttached() {}

    @Override
    public void onSubpageRemoved() {}

    public void setPermissions(PageInfoView.PermissionParams params) {
        mTitle = mRowView.getContext().getResources().getString(
                R.string.page_info_permissions_title);
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = true;
        rowParams.title = mTitle;
        // TODO(crbug.com/1077766): Create a permissions subtitle string that represents
        // the state, using the PageInfoView.PermissionParams and potentially R.plurals.
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }
}

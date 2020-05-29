// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

/**
 * Class for controlling the page info connection section.
 */
public class PageInfoConnectionController implements PageInfoSubpageController {
    private PageInfoController mMainController;
    private PageInfoViewV2 mView;
    private String mTitle;

    public PageInfoConnectionController(PageInfoController mainController, PageInfoViewV2 view) {
        mMainController = mainController;
        mView = view;
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
        // TODO(crbug.com/1077766): Create and set the connection specific view.
        return null;
    }

    @Override
    public void willRemoveSubpage() {}

    public void setConnectionInfo(PageInfoView.ConnectionInfoParams params) {
        mTitle = params.summary != null ? params.summary.toString() : null;
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = mTitle;
        rowParams.subtitle = params.message != null ? params.message.toString() : null;
        rowParams.visible = rowParams.title != null || rowParams.subtitle != null;
        rowParams.clickCallback = this::launchSubpage;
        mView.getConnectionRowView().setParams(rowParams);
    }
}

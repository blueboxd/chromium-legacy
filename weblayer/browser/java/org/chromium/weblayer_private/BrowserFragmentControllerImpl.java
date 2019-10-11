// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IProfile;

/**
 * Implementation of {@link IBrowserFragmentController}.
 */
public class BrowserFragmentControllerImpl extends IBrowserFragmentController.Stub {
    private final ProfileImpl mProfile;
    private BrowserControllerImpl mTabController;

    public BrowserFragmentControllerImpl(ProfileImpl profile, Bundle savedInstanceState) {
        mProfile = profile;
        // Restore tabs etc from savedInstanceState here.
    }

    public void onFragmentAttached(Context context) {
        mTabController = new BrowserControllerImpl(context, mProfile);
    }

    public void onFragmentDetached() {
        mTabController.destroy();
        mTabController = null;
    }

    @Override
    public void setTopView(IObjectWrapper view) {
        getBrowserController().setTopView(view);
    }

    @Override
    public void setSupportsEmbedding(boolean enable, IObjectWrapper valueCallback) {
        getBrowserController().setSupportsEmbedding(enable, valueCallback);
    }

    @Override
    public BrowserControllerImpl getBrowserController() {
        if (mTabController == null) {
            throw new RuntimeException("Currently BrowserController requires Activity context, so "
                    + "it exists only while BrowserFragment is attached to an Activity");
        }
        return mTabController;
    }

    @Override
    public IProfile getProfile() {
        return mProfile;
    }

    public View getFragmentView() {
        return mTabController.getView();
    }

    public void destroy() {}
}

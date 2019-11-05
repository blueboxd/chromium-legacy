// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

import java.util.ArrayList;

/**
 * Implementation of {@link IBrowserFragmentController}.
 */
public class BrowserFragmentControllerImpl extends IBrowserFragmentController.Stub {
    private final ProfileImpl mProfile;
    private BrowserFragmentViewController mViewController;
    private FragmentWindowAndroid mWindowAndroid;
    private ArrayList<BrowserControllerImpl> mBrowsers = new ArrayList<BrowserControllerImpl>();

    public BrowserFragmentControllerImpl(ProfileImpl profile, Bundle savedInstanceState) {
        mProfile = profile;
        // Restore tabs etc from savedInstanceState here.
    }

    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    public ViewGroup getViewAndroidDelegateContainerView() {
        return mViewController.getContentView();
    }

    public void onFragmentAttached(Context context, FragmentWindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        mViewController = new BrowserFragmentViewController(context, windowAndroid);
        BrowserControllerImpl browser = new BrowserControllerImpl(mProfile, windowAndroid);
        attachBrowserController(browser);
        boolean set_active_result = setActiveBrowserController(browser);
        assert set_active_result;
    }

    public void onFragmentDetached() {
        destroy(); // For now we don't retain anything between detach and attach.
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mWindowAndroid != null) {
            mWindowAndroid.onActivityResult(requestCode, resultCode, data);
        }
    }

    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (mWindowAndroid != null) {
            mWindowAndroid.handlePermissionResult(requestCode, permissions, grantResults);
        }
    }

    @Override
    public void setTopView(IObjectWrapper viewWrapper) {
        getViewController().setTopView(ObjectWrapper.unwrap(viewWrapper, View.class));
    }

    @Override
    public void setSupportsEmbedding(boolean enable, IObjectWrapper valueCallback) {
        getViewController().setSupportsEmbedding(enable,
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(valueCallback, ValueCallback.class));
    }

    @Override
    public BrowserControllerImpl getBrowserController() {
        return getViewController().getBrowserController();
    }

    public BrowserFragmentViewController getViewController() {
        if (mViewController == null) {
            throw new RuntimeException("Currently BrowserController requires Activity context, so "
                    + "it exists only while BrowserFragment is attached to an Activity");
        }
        return mViewController;
    }

    @Override
    public IProfile getProfile() {
        return mProfile;
    }

    public void attachBrowserController(IBrowserController iBrowserController) {
        BrowserControllerImpl browserController = (BrowserControllerImpl) iBrowserController;
        if (browserController.getFragment() == this) return;
        attachBrowserControllerImpl(browserController);
    }

    private void attachBrowserControllerImpl(BrowserControllerImpl browser) {
        // Null case is only during initial creation.
        if (browser.getFragment() != this && browser.getFragment() != null) {
            browser.getFragment().detachBrowserController(browser);
        }
        mBrowsers.add(browser);
        browser.attachToFragment(this);
    }

    public void detachBrowserController(IBrowserController controller) {
        BrowserControllerImpl browser = (BrowserControllerImpl) controller;
        if (browser.getFragment() != this) return;
        if (getActiveBrowserController() == browser) setActiveBrowserController(null);
        mBrowsers.remove(browser);
        // This doesn't reset state on BrowserControllerImpl as |browser| is either about to be
        // destroyed, or switching to a different fragment.
    }

    public boolean setActiveBrowserController(BrowserControllerImpl browser) {
        if (browser.getFragment() != this) return false;
        mViewController.setActiveBrowserController(browser);
        return true;
    }

    public BrowserControllerImpl getActiveBrowserController() {
        return mViewController.getBrowserController();
    }

    public View getFragmentView() {
        return getViewController().getView();
    }

    public void destroy() {
        if (mViewController != null) {
            mViewController.destroy();
            for (BrowserControllerImpl browser : mBrowsers) {
                browser.destroy();
            }
            mViewController = null;
        }
        mWindowAndroid = null;
        mViewController = null;
    }
}

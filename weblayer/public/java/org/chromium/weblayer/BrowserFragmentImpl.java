// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;
import android.view.View;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Provides an API similar to that of Fragment. To avoid depending upon a particular Fragment
 * implementation, this class does not actually extend Fragment. It is expected that consumers of
 * this provide an implementation of Fragment that calls through to the similarly named methods in
 * this class.
 *
 * BrowserFragmentImpl is created from Profile.
 */
public final class BrowserFragmentImpl {
    private BrowserController mBrowserController;

    BrowserFragmentImpl(IBrowserController iBrowserController) {
        mBrowserController = new BrowserController(iBrowserController);
    }

    public void destroy() {
        mBrowserController.destroy();
    }

    public void setTopView(View view) {
        try {
            getIBrowserController().setTopView(ObjectWrapper.wrap(view));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public View onCreateView() {
        try {
            return ObjectWrapper.unwrap(getIBrowserController().onCreateView(), View.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public BrowserController getBrowserController() {
        return mBrowserController;
    }

    private IBrowserController getIBrowserController() {
        return mBrowserController.getIBrowserController();
    }
}

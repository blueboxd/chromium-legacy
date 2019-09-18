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

    /**
     * Control support for embedding use cases such as animations. This should be enabled when the
     * container view of the fragment is animated in any way, needs to be rotated or blended, or
     * need to control z-order with other views or other BrowserFragmentImpls. Note embedder should
     * keep WebLayer in the default non-embedding mode when user is interacting with the web
     * content.
     */
    public void setSupportsEmbedding(boolean enable) {
        try {
            getIBrowserController().setSupportsEmbedding(enable);
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

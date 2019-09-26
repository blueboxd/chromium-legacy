// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.view.View;

import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IRemoteFragment;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;

/**
 * Implementation of {@link IBrowserFragmentController}.
 */
public class BrowserFragmentControllerImpl extends IBrowserFragmentController.Stub {
    private final BrowserControllerImpl mController;
    private final BrowserRemoteFragmentImpl mRemoteFragmentImpl;

    public BrowserFragmentControllerImpl(BrowserControllerImpl controller,
            IRemoteFragmentClient fragmentClient) {
        mController = controller;
        mRemoteFragmentImpl = new BrowserRemoteFragmentImpl(fragmentClient);
    }

    @Override
    public void setTopView(IObjectWrapper view) {
        mController.setTopView(view);
    }

    @Override
    public void setSupportsEmbedding(boolean enable, IObjectWrapper valueCallback) {
        mController.setSupportsEmbedding(enable, valueCallback);
    }

    @Override
    public IRemoteFragment getRemoteFragment() {
        return mRemoteFragmentImpl;
    }

    @Override
    public IBrowserController getBrowserController() {
        return mController;
    }

    @Override
    public void destroy() {
        mController.destroy();
    }

    private class BrowserRemoteFragmentImpl extends RemoteFragmentImpl {
        public BrowserRemoteFragmentImpl(IRemoteFragmentClient client) {
            super(client);
        }

        @Override
        public View onCreateView() {
            return mController.getView();
        }
    }
}

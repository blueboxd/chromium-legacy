// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;
import android.view.View;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Represents a browser fragment. Created from Profile.
 */
public final class BrowserFragmentController {
    private final IBrowserFragmentController mImpl;
    private final ProfileManager mProfileManager;
    private BrowserController mController;


    BrowserFragmentController(IBrowserFragmentController impl, ProfileManager profileManager) {
        mImpl = impl;
        mProfileManager = profileManager;
    }

    // TODO(pshmakov): rename this to BrowserTabController.
    @NonNull
    public BrowserController getBrowserController() {
        if (mController == null) {
            try {
                mController = new BrowserController(mImpl.getBrowserController());
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        return mController;
    }

    public void setTopView(@Nullable View view) {
        try {
            mImpl.setTopView(ObjectWrapper.wrap(view));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Control support for embedding use cases such as animations. This should be enabled when the
     * container view of the fragment is animated in any way, needs to be rotated or blended, or
     * need to control z-order with other views or other BrowserFragmentImpls. Note embedder should
     * keep WebLayer in the default non-embedding mode when user is interacting with the web
     * content. Embedding mode does not support encrypted video.
     * @return a ListenableResult of whether the request succeeded. A request might fail if it is
     * subsumed by a subsequent request, or if this object is destroyed.
     */
    @NonNull
    public ListenableResult<Boolean> setSupportsEmbedding(boolean enable) {
        try {
            final ListenableResult<Boolean> listenableResult = new ListenableResult<Boolean>();
            mImpl.setSupportsEmbedding(
                    enable, ObjectWrapper.wrap(new ValueCallback<Boolean>() {
                        @Override
                        public void onReceiveValue(Boolean result) {
                            listenableResult.supplyResult(result);
                        }
                    }));
            return listenableResult;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns {@link Profile} associated with this Browser Fragment. Multiple fragments can share
     * the same Profile.
     */
    @NonNull
    public Profile getProfile() {
        try {
            return mProfileManager.getProfileFor(mImpl.getProfile());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}

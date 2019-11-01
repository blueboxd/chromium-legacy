// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;
import android.support.v4.app.Fragment;
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
    private BrowserController mController;

    BrowserFragmentController(IBrowserFragmentController impl) {
        mImpl = impl;
    }

    /**
     * Returns the BrowserFragmentController for the supplied Fragment; null if
     * {@link fragment} was not created by WebLayer.
     *
     * @return the BrowserFragmentController
     */
    @Nullable
    public static BrowserFragmentController fromFragment(@Nullable Fragment fragment) {
        return fragment instanceof BrowserFragment ? ((BrowserFragment) fragment).getController()
                                                   : null;
    }

    /**
     * Returns the BrowserController associated with this BrowserFragmentController.
     *
     * @return The BrowserController.
     */
    @NonNull
    public BrowserController getBrowserController() {
        ThreadCheck.ensureOnUiThread();
        if (mController == null) {
            try {
                mController = new BrowserController(mImpl.getBrowserController());
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        return mController;
    }

    /**
     * Sets the View shown at the top of the browser. A value of null removes the view. The
     * top-view is typically used to show the uri. The top-view scrolls with the page.
     *
     * @param view The new top-view.
     */
    public void setTopView(@Nullable View view) {
        ThreadCheck.ensureOnUiThread();
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
        ThreadCheck.ensureOnUiThread();
        try {
            final ListenableResult<Boolean> listenableResult = new ListenableResult<Boolean>();
            mImpl.setSupportsEmbedding(enable, ObjectWrapper.wrap(new ValueCallback<Boolean>() {
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
        ThreadCheck.ensureOnUiThread();
        try {
            return Profile.of(mImpl.getProfile());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}

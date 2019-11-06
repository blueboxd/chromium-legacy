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
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * BrowserFragmentController contains any number of BrowserControllers, with one active
 * BrowserController. The active BrowserController is visible to the user, all other
 * BrowserControllers are hidden.
 *
 * By default BrowserFragmentController has a single active BrowserController.
 */
public final class BrowserFragmentController {
    private final IBrowserFragmentController mImpl;
    // Mapes from id (as returned from IBrowserController.getId() to BrowserController.
    private final Map<Integer, BrowserController> mBrowserControllerMap;

    BrowserFragmentController(IBrowserFragmentController impl) {
        mImpl = impl;
        mBrowserControllerMap = new HashMap<Integer, BrowserController>();
        try {
            for (Object browserController : impl.getBrowserControllers()) {
                // getBrowserControllers() returns List<BrowserControllerImpl>, which isn't
                // accessible from the client library.
                IBrowserController iBrowserController =
                        IBrowserController.Stub.asInterface((android.os.IBinder) browserController);
                // BrowserController's constructor calls registerBrowserController().
                new BrowserController(iBrowserController, this);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    // This is called when a new BrowserController is created.
    void registerBrowserController(BrowserController browserController) {
        try {
            int id = browserController.getIBrowserController().getId();
            assert !mBrowserControllerMap.containsKey(id);
            mBrowserControllerMap.put(id, browserController);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
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
     * Sets the active (visible) BrowserController. Only one BrowserController is visible at a time.
     *
     * @param browserController The BrowserController to make active.
     *
     * @throws IllegalStateException if {@link browserController} was not added to this
     *         BrowserFragmentController.
     *
     * @see #addBrowserController()
     */
    public void setActiveBrowserController(@NonNull BrowserController browserController) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (!mImpl.setActiveBrowserController(browserController.getIBrowserController())) {
                throw new IllegalStateException("attachBrowserController() must be called before "
                        + "setActiveBrowserController");
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the active (visible) BrowserController associated with this
     * BrowserFragmentController.
     *
     * @return The BrowserController.
     */
    @Nullable
    public BrowserController getActiveBrowserController() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mBrowserControllerMap.get(mImpl.getActiveBrowserControllerId());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the set of BrowserControllers contained in this BrowserFragmentController.
     *
     * @return The BrowserControllers
     */
    @NonNull
    public List<BrowserController> getBrowserControllers() {
        ThreadCheck.ensureOnUiThread();
        return new ArrayList<BrowserController>(mBrowserControllerMap.values());
    }

    /**
     * Disposes a BrowserController. If {@link browserController} is the active BrowserController,
     * no BrowserController is made active. After this call {@link browserController} should not be
     * used.
     *
     * @param browserController The BrowserController to dispose.
     *
     * @throws IllegalStateException is {@link browserController} is not in this
     *         BrowserFragmentController.
     */
    public void disposeBrowserController(BrowserController browserController) {
        ThreadCheck.ensureOnUiThread();
        // TODO(sky): implement this.
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

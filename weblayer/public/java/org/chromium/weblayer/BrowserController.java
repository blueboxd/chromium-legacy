// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.util.Log;
import android.view.View;

import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

import java.util.concurrent.CopyOnWriteArrayList;

public final class BrowserController {
    private static final String TAG = "WL_BrowserController";

    private final IBrowserController mImpl;
    private final NavigationController mNavigationController;
    // TODO(sky): copy ObserverList from base and use it instead.
    private final CopyOnWriteArrayList<BrowserObserver> mObservers;

    public BrowserController(IBrowserController impl) {
        mImpl = impl;
        try {
            mImpl.setClient(new BrowserClientImpl());
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call setClient.", e);
            throw new AndroidRuntimeException(e);
        }

        mNavigationController = new NavigationController(this);
        mObservers = new CopyOnWriteArrayList<BrowserObserver>();
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mProfile is non-null.
    }

    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    public void setTopView(View view) {
        try {
            mImpl.setTopView(ObjectWrapper.wrap(view));
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call setTopView.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    public void addObserver(BrowserObserver observer) {
        mObservers.add(observer);
    }

    public void removeObserver(BrowserObserver observer) {
        mObservers.remove(observer);
    }

    public void destroy() {
        try {
            mImpl.destroy();
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call destroy.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    public View onCreateView() {
        try {
            return ObjectWrapper.unwrap(mImpl.onCreateView(), View.class);
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call onCreateView.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    void navigate(Uri uri) {
        try {
            mImpl.navigate(uri.toString());
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call navigate.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    private final class BrowserClientImpl extends IBrowserControllerClient.Stub {
        @Override
        public void displayURLChanged(String url) {
            Uri uri = Uri.parse(url);
            for (BrowserObserver observer : mObservers) {
                observer.displayURLChanged(uri);
            }
        }
    }
}

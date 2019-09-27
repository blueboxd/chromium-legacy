// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;

public final class BrowserController {
    private final IBrowserController mImpl;
    private final NavigationController mNavigationController;
    private final ObserverList<BrowserObserver> mObservers;

    BrowserController(IBrowserController impl) {
        mImpl = impl;
        try {
            mImpl.setClient(new BrowserClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }

        mObservers = new ObserverList<BrowserObserver>();
        mNavigationController = NavigationController.create(mImpl);
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mProfile is non-null.
    }

    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    public void addObserver(BrowserObserver observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(BrowserObserver observer) {
        mObservers.removeObserver(observer);
    }

    IBrowserController getIBrowserController() {
        return mImpl;
    }

    private final class BrowserClientImpl extends IBrowserControllerClient.Stub {
        @Override
        public void visibleUrlChanged(String url) {
            Uri uri = Uri.parse(url);
            for (BrowserObserver observer : mObservers) {
                observer.visibleUrlChanged(uri);
            }
        }

        @Override
        public void loadingStateChanged(boolean isLoading, boolean toDifferentDocument) {
            for (BrowserObserver observer : mObservers) {
                observer.loadingStateChanged(isLoading, toDifferentDocument);
            }
        }

        @Override
        public void loadProgressChanged(double progress) {
            for (BrowserObserver observer : mObservers) {
                observer.loadProgressChanged(progress);
            }
        }
    }
}

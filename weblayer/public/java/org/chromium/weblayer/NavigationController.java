// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IClientNavigation;
import org.chromium.weblayer_private.aidl.INavigation;
import org.chromium.weblayer_private.aidl.INavigationController;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;

/**
 * Provides methods to control navigation, along with maintaining the current list of navigations.
 */
public final class NavigationController {
    private INavigationController mNavigationController;
    private final ObserverList<NavigationObserver> mObservers;

    static NavigationController create(IBrowserController browserController) {
        NavigationController navigationController = new NavigationController();
        try {
            navigationController.mNavigationController =
                    browserController.createNavigationController(
                            navigationController.new NavigationControllerClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return navigationController;
    }

    private NavigationController() {
        mObservers = new ObserverList<NavigationObserver>();
    }

    public void navigate(Uri uri) {
        try {
            mNavigationController.navigate(uri.toString());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void goBack() {
        try {
            mNavigationController.goBack();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void goForward() {
        try {
            mNavigationController.goForward();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void reload() {
        try {
            mNavigationController.reload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void stop() {
        try {
            mNavigationController.stop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public int getNavigationListSize() {
        try {
            return mNavigationController.getNavigationListSize();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public int getNavigationListCurrentIndex() {
        try {
            return mNavigationController.getNavigationListCurrentIndex();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public Uri getNavigationEntryDisplayUri(int index) {
        try {
            return Uri.parse(mNavigationController.getNavigationEntryDisplayUri(index));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void addObserver(NavigationObserver observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(NavigationObserver observer) {
        mObservers.removeObserver(observer);
    }

    private final class NavigationControllerClientImpl extends INavigationControllerClient.Stub {
        @Override
        public IClientNavigation createClientNavigation(INavigation navigationImpl) {
            return new Navigation(navigationImpl);
        }

        @Override
        public void navigationStarted(IClientNavigation navigation) {
            for (NavigationObserver observer : mObservers) {
                observer.navigationStarted((Navigation) navigation);
            }
        }

        @Override
        public void navigationRedirected(IClientNavigation navigation) {
            for (NavigationObserver observer : mObservers) {
                observer.navigationRedirected((Navigation) navigation);
            }
        }

        @Override
        public void navigationCommitted(IClientNavigation navigation) {
            for (NavigationObserver observer : mObservers) {
                observer.navigationCommitted((Navigation) navigation);
            }
        }

        @Override
        public void navigationCompleted(IClientNavigation navigation) {
            for (NavigationObserver observer : mObservers) {
                observer.navigationCompleted((Navigation) navigation);
            }
        }

        @Override
        public void navigationFailed(IClientNavigation navigation) {
            for (NavigationObserver observer : mObservers) {
                observer.navigationFailed((Navigation) navigation);
            }
        }
    }
}

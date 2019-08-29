// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Activity;
import android.net.Uri;
import android.view.View;

import org.chromium.weblayer_private.BrowserControllerClient;
import org.chromium.weblayer_private.BrowserControllerImpl;

import java.util.concurrent.CopyOnWriteArrayList;

public final class BrowserController {
    private final BrowserControllerImpl mBrowserController;
    private final NavigationController mNavigationController;
    // TODO(sky): copy ObserverList from base and use it instead.
    private final CopyOnWriteArrayList<BrowserObserver> mObservers;

    public BrowserController(Activity activity, Profile profile) {
        mBrowserController = new BrowserControllerImpl(
                activity, profile.getProfileImpl(), new BrowserClientImpl());
        mNavigationController = new NavigationController(mBrowserController);
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
        mBrowserController.setTopView(view);
    }

    public void addObserver(BrowserObserver observer) {
        mObservers.add(observer);
    }

    public void removeObserver(BrowserObserver observer) {
        mObservers.remove(observer);
    }

    private final class BrowserClientImpl implements BrowserControllerClient {
        @Override
        public void displayURLChanged(String url) {
            Uri uri = Uri.parse(url);
            for (BrowserObserver observer : mObservers) {
                observer.displayURLChanged(uri);
            }
        }
    }
}

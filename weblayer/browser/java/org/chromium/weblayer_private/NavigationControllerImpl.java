// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.INavigationController;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;

/**
 * Acts as the bridge between java and the C++ implementation of of NavigationController.
 */
@JNINamespace("weblayer")
public final class NavigationControllerImpl extends INavigationController.Stub {
    private long mNativeNavigationController;
    private BrowserControllerImpl mBrowserController;
    private INavigationControllerClient mNavigationControllerClient;

    public NavigationControllerImpl(
            BrowserControllerImpl browserController, INavigationControllerClient client) {
        mNavigationControllerClient = client;
        mBrowserController = browserController;
        mNativeNavigationController =
                nativeGetNavigationController(browserController.getNativeBrowserController());
        nativeSetNavigationControllerImpl(mNativeNavigationController);
    }

    @Override
    public void navigate(String uri) {
        nativeNavigate(mNativeNavigationController, uri);
    }

    @Override
    public void goBack() {
        nativeGoBack(mNativeNavigationController);
    }

    @Override
    public void goForward() {
        nativeGoForward(mNativeNavigationController);
    }

    @Override
    public void reload() {
        nativeReload(mNativeNavigationController);
    }

    @Override
    public void stop() {
        nativeStop(mNativeNavigationController);
    }

    @Override
    public int getNavigationListSize() {
        return nativeGetNavigationListSize(mNativeNavigationController);
    }

    @Override
    public int getNavigationListCurrentIndex() {
        return nativeGetNavigationListCurrentIndex(mNativeNavigationController);
    }

    @Override
    public String getNavigationEntryDisplayUri(int index) {
        return nativeGetNavigationEntryDisplayUri(mNativeNavigationController, index);
    }

    @CalledByNative
    private NavigationImpl createNavigation(long nativeNavigationImpl) {
        return new NavigationImpl(mNavigationControllerClient, nativeNavigationImpl);
    }

    @CalledByNative
    private void navigationStarted(NavigationImpl navigation) {
        try {
            mNavigationControllerClient.navigationStarted(navigation.getClientNavigation());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void navigationRedirected(NavigationImpl navigation) {
        try {
            mNavigationControllerClient.navigationRedirected(navigation.getClientNavigation());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void navigationCommitted(NavigationImpl navigation) {
        try {
            mNavigationControllerClient.navigationCommitted(navigation.getClientNavigation());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void navigationCompleted(NavigationImpl navigation) {
        try {
            mNavigationControllerClient.navigationCompleted(navigation.getClientNavigation());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void navigationFailed(NavigationImpl navigation) {
        try {
            mNavigationControllerClient.navigationFailed(navigation.getClientNavigation());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private native void nativeSetNavigationControllerImpl(long nativeNavigationControllerImpl);

    private static native long nativeGetNavigationController(long browserController);
    private native void nativeNavigate(long nativeNavigationControllerImpl, String uri);
    private native void nativeGoBack(long nativeNavigationControllerImpl);
    private native void nativeGoForward(long nativeNavigationControllerImpl);
    private native void nativeReload(long nativeNavigationControllerImpl);
    private native void nativeStop(long nativeNavigationControllerImpl);
    private native int nativeGetNavigationListSize(long nativeNavigationControllerImpl);
    private native int nativeGetNavigationListCurrentIndex(long nativeNavigationControllerImpl);
    private native String nativeGetNavigationEntryDisplayUri(
            long nativeNavigationControllerImpl, int index);
}

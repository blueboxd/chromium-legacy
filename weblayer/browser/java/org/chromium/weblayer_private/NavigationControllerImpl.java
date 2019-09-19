// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
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
        mNativeNavigationController = NavigationControllerImplJni.get().getNavigationController(
                browserController.getNativeBrowserController());
        NavigationControllerImplJni.get().setNavigationControllerImpl(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void navigate(String uri) {
        NavigationControllerImplJni.get().navigate(
                mNativeNavigationController, NavigationControllerImpl.this, uri);
    }

    @Override
    public void goBack() {
        NavigationControllerImplJni.get().goBack(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void goForward() {
        NavigationControllerImplJni.get().goForward(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void reload() {
        NavigationControllerImplJni.get().reload(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void stop() {
        NavigationControllerImplJni.get().stop(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public int getNavigationListSize() {
        return NavigationControllerImplJni.get().getNavigationListSize(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public int getNavigationListCurrentIndex() {
        return NavigationControllerImplJni.get().getNavigationListCurrentIndex(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public String getNavigationEntryDisplayUri(int index) {
        return NavigationControllerImplJni.get().getNavigationEntryDisplayUri(
                mNativeNavigationController, NavigationControllerImpl.this, index);
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

    @NativeMethods
    interface Natives {
        void setNavigationControllerImpl(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        long getNavigationController(long browserController);
        void navigate(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller, String uri);
        void goBack(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        void goForward(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        void reload(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        void stop(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        int getNavigationListSize(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        int getNavigationListCurrentIndex(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        String getNavigationEntryDisplayUri(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller, int index);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Application;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.webkit.ValueCallback;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IWebLayer;
import org.chromium.weblayer_private.aidl.ObjectWrapper;
import org.chromium.weblayer_private.aidl.WebLayerVersion;

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.util.concurrent.TimeUnit;

/**
 * WebLayer is responsible for initializing state necessary to use any of the classes in web layer.
 */
public final class WebLayer {
    // TODO: Using a metadata key for the WebLayerImpl package is just being used for testing,
    // production will use a different mechanism.
    private static final String PACKAGE_MANIFEST_KEY = "org.chromium.weblayer.WebLayerPackage";

    private static ListenableFuture<WebLayer> sFuture;

    private IWebLayer mImpl;

    /**
     * Loads the WebLayer implementation and returns the IWebLayer. This does *not* trigger the
     * implementation to start.
     */
    private static IWebLayer connectToWebLayerImplementation(Application application)
            throws UnsupportedVersionException {
        try {
            // TODO: Make asset loading work on L, where WebViewDelegate doesn't exist.
            // WebViewDelegate.addWebViewAssetPath() accesses the currently loaded package info from
            // WebViewFactory, so we have to fake it.
            PackageInfo implPackageInfo = application.getPackageManager().getPackageInfo(
                    getImplPackageName(application), PackageManager.GET_META_DATA);
            Field packageInfo = WebViewFactory.class.getDeclaredField("sPackageInfo");
            packageInfo.setAccessible(true);
            packageInfo.set(null, implPackageInfo);

            // TODO(torne): Figure out how to load assets for production.
            // Load assets using the WebViewDelegate.
            Constructor constructor = WebViewDelegate.class.getDeclaredConstructor();
            constructor.setAccessible(true);
            WebViewDelegate delegate = (WebViewDelegate) constructor.newInstance();
            delegate.addWebViewAssetPath(application);

            Context remoteContext = createRemoteContext(application);
            Class webLayerClass = remoteContext.getClassLoader().loadClass(
                    "org.chromium.weblayer_private.WebLayerImpl");

            // Check version before doing anything else on the implementation side.
            if (!(boolean) webLayerClass.getMethod("checkVersion", Integer.TYPE)
                            .invoke(null, WebLayerVersion.sVersionNumber)) {
                throw new UnsupportedVersionException(WebLayerVersion.sVersionNumber);
            }

            return IWebLayer.Stub.asInterface(
                    (IBinder) webLayerClass.getMethod("create").invoke(null));
        } catch (UnsupportedVersionException e) {
            throw e;
        } catch (Exception e) {
            throw new APICallException(e);
        }
    }

    /**
     * Asynchronously creates and initializes WebLayer. Calling this more than once returns the same
     * object.
     *
     * @param application The hosting Application
     * @return a ListenableFuture whose value will contain the WebLayer once initialization
     * completes
     */
    public static ListenableFuture<WebLayer> create(Application application)
            throws UnsupportedVersionException {
        if (sFuture == null) {
            IWebLayer iWebLayer = connectToWebLayerImplementation(application);
            sFuture = new WebLayerLoadFuture(iWebLayer, application);
        }
        return sFuture;
    }

    /**
     * Future that creates WebLayer once the implementation has completed startup.
     */
    private static final class WebLayerLoadFuture extends ListenableFuture<WebLayer> {
        private final IWebLayer mIWebLayer;

        WebLayerLoadFuture(IWebLayer iWebLayer, Application application) {
            mIWebLayer = iWebLayer;
            ValueCallback<Boolean> loadCallback = new ValueCallback<Boolean>() {
                @Override
                public void onReceiveValue(Boolean result) {
                    // TODO: figure out when |result| is false and what to do in such a scenario.
                    assert result;
                    supplyResult(new WebLayer(mIWebLayer));
                }
            };
            try {
                iWebLayer.initAndLoadAsync(ObjectWrapper.wrap(createRemoteContext(application)),
                        ObjectWrapper.wrap(loadCallback));
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        @Override
        public boolean cancel(boolean mayInterruptIfRunning) {
            // Loading can not be canceled.
            return false;
        }

        @Override
        public WebLayer get(long timeout, TimeUnit unit) {
            // Arbitrary timeouts are not supported.
            throw new UnsupportedOperationException();
        }

        @Override
        public void onLoad() {
            try {
                mIWebLayer.loadSync();
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        @Override
        public boolean isCancelled() {
            return false;
        }
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mImpl is non-null.
    }

    public void destroy() {
        // TODO: implement me.
    }

    private WebLayer(IWebLayer iWebLayer) {
        mImpl = iWebLayer;
    }

    /**
     * Creates a new Profile with the given path. Pass in an null path for an in-memory profile.
     */
    public Profile createProfile(File path) {
        try {
            return new Profile(mImpl.createProfile(path == null ? "" : path.getPath()));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Creates a Context for the remote (weblayer implementation) side.
     */
    static Context createRemoteContext(Context localContext) {
        Context remoteContext;
        try {
            // TODO(cduvall): Might want to cache the remote context so we don't need to call into
            // package manager more than we need to.
            remoteContext = localContext.createPackageContext(getImplPackageName(localContext),
                    Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (NameNotFoundException e) {
            throw new AndroidRuntimeException(e);
        }
        return wrapContext(localContext, remoteContext);
    }

    private static Context wrapContext(Context localContext, Context remoteContext) {
        return new ContextWrapper(localContext) {
            @Override
            public Context getApplicationContext() {
                if (getBaseContext().getApplicationContext() == getBaseContext()) return this;
                return wrapContext(getBaseContext().getApplicationContext(), remoteContext);
            }

            @Override
            public Resources getResources() {
                return remoteContext.getResources();
            }

            @Override
            public ClassLoader getClassLoader() {
                return remoteContext.getClassLoader();
            }

            @Override
            public void registerComponentCallbacks(ComponentCallbacks callback) {
                // We have to override registerComponentCallbacks and unregisterComponentCallbacks
                // since they call getApplicationContext().[un]registerComponentCallbacks()
                // which causes us to go into a loop.
                getBaseContext().registerComponentCallbacks(callback);
            }

            @Override
            public void unregisterComponentCallbacks(ComponentCallbacks callback) {
                getBaseContext().unregisterComponentCallbacks(callback);
            }
        };
    }

    private static String getImplPackageName(Context localContext)
            throws PackageManager.NameNotFoundException {
        Bundle metaData = localContext.getPackageManager()
                                  .getApplicationInfo(localContext.getPackageName(),
                                          PackageManager.GET_META_DATA)
                                  .metaData;
        if (metaData != null) return metaData.getString(PACKAGE_MANIFEST_KEY);
        return null;
    }
}

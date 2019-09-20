// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Application;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IWebLayer;

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;

/**
 * WebLayer is responsible for initializing state necessary to use* any of the classes in web layer.
 */
public final class WebLayer {
    // TODO: Using a metadata key for the WebLayerImpl package is just being used for testing,
    // production will use a different mechanism.
    private static final String PACKAGE_MANIFEST_KEY = "org.chromium.weblayer.WebLayerPackage";

    private static WebLayer sInstance;
    private IWebLayer mImpl;

    public static WebLayer getInstance() {
        if (sInstance == null) {
            sInstance = new WebLayer();
        }
        return sInstance;
    }

    WebLayer() {}

    public void init(Application application) {
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
            mImpl = IWebLayer.Stub.asInterface(
                    (IBinder) remoteContext.getClassLoader()
                            .loadClass("org.chromium.weblayer_private.WebLayerImpl")
                            .getMethod("create", Application.class, Context.class)
                            .invoke(null, application, remoteContext));
        } catch (Exception e) {
            throw new APICallException(e);
        }
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mImpl is non-null.
    }

    public void destroy() {
        // TODO: implement me.
        sInstance = null;
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
        try {
            // TODO(cduvall): Might want to cache the remote context so we don't need to call into
            // package manager more than we need to.
            return localContext.createPackageContext(getImplPackageName(localContext),
                    Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (NameNotFoundException e) {
            throw new AndroidRuntimeException(e);
        }
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

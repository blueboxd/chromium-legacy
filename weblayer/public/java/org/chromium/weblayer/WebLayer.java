// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Application;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.RemoteException;
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
    private static final String PACKAGE_NAME = "org.chromium.weblayer.support";

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
                    PACKAGE_NAME, PackageManager.GET_META_DATA);
            Field packageInfo = WebViewFactory.class.getDeclaredField("sPackageInfo");
            packageInfo.setAccessible(true);
            packageInfo.set(null, implPackageInfo);

            // TODO(torne): Figure out how to load assets for production.
            // Load assets using the WebViewDelegate.
            Constructor constructor = WebViewDelegate.class.getDeclaredConstructor();
            constructor.setAccessible(true);
            WebViewDelegate delegate = (WebViewDelegate) constructor.newInstance();
            delegate.addWebViewAssetPath(application);

            Context remoteContext = application.createPackageContext(
                    PACKAGE_NAME, Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
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
}

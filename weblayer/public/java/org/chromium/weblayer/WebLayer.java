// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.webkit.ValueCallback;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.BrowserFragmentArgs;
import org.chromium.weblayer_private.aidl.IBrowserFragment;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;
import org.chromium.weblayer_private.aidl.IWebLayer;
import org.chromium.weblayer_private.aidl.ObjectWrapper;
import org.chromium.weblayer_private.aidl.WebLayerVersion;

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

    private final IWebLayer mImpl;
    private final ProfileManager mProfileManager = new ProfileManager();

    /**
     * Loads the WebLayer implementation and returns the IWebLayer. This does *not* trigger the
     * implementation to start.
     */
    private static IWebLayer connectToWebLayerImplementation(Context remoteContext)
            throws UnsupportedVersionException {
        try {
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
     * Loads assets for WebLayer and returns the package ID to use when calling
     * R.onResourcesLoaded().
     */
    private static int loadAssets(Context appContext, Context remoteContext) {
        WebViewDelegate delegate;
        PackageInfo implPackageInfo;
        try {
            // TODO: Make asset loading work on L, where WebViewDelegate doesn't exist.
            // WebViewDelegate.addWebViewAssetPath() accesses the currently loaded package info from
            // WebViewFactory, so we have to fake it.
            implPackageInfo =
                    appContext.getPackageManager().getPackageInfo(getImplPackageName(appContext),
                            PackageManager.GET_SHARED_LIBRARY_FILES | PackageManager.GET_META_DATA);
            Field packageInfo = WebViewFactory.class.getDeclaredField("sPackageInfo");
            packageInfo.setAccessible(true);
            packageInfo.set(null, implPackageInfo);

            // TODO(torne): Figure out how to load assets for production.
            // Load assets using the WebViewDelegate.
            Constructor constructor = WebViewDelegate.class.getDeclaredConstructor();
            constructor.setAccessible(true);
            delegate = (WebViewDelegate) constructor.newInstance();
        } catch (Exception e) {
            throw new AndroidRuntimeException(e);
        }
        delegate.addWebViewAssetPath(appContext);
        return delegate.getPackageId(appContext.getResources(), implPackageInfo.packageName);
    }

    /**
     * Asynchronously creates and initializes WebLayer. Calling this more than once returns the same
     * object.
     *
     * @param appContext The hosting application's Context.
     * @return a ListenableFuture whose value will contain the WebLayer once initialization
     * completes
     */
    public static ListenableFuture<WebLayer> create(Context appContext)
            throws UnsupportedVersionException {
        if (sFuture == null) {
            Context remoteContext = createRemoteContext(appContext.getApplicationContext());
            IWebLayer iWebLayer = connectToWebLayerImplementation(remoteContext);
            int resourcesPackageId = loadAssets(appContext, remoteContext);
            sFuture = new WebLayerLoadFuture(iWebLayer, remoteContext, resourcesPackageId);
        }
        return sFuture;
    }

    /**
     * Future that creates WebLayer once the implementation has completed startup.
     */
    private static final class WebLayerLoadFuture extends ListenableFuture<WebLayer> {
        private final IWebLayer mIWebLayer;

        WebLayerLoadFuture(IWebLayer iWebLayer, Context remoteContext, int resourcesPackageId) {
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
                iWebLayer.initAndLoadAsync(ObjectWrapper.wrap(remoteContext),
                        ObjectWrapper.wrap(loadCallback), resourcesPackageId);
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
        mProfileManager.destroy();
    }

    private WebLayer(IWebLayer iWebLayer) {
        mImpl = iWebLayer;
    }

    public static BrowserFragment createBrowserFragment(String profilePath) {
        // TODO: use a profile id instead of the path to the actual file.
        Bundle args = new Bundle();
        args.putString(BrowserFragmentArgs.PROFILE_PATH, profilePath == null ? "" : profilePath);
        BrowserFragment fragment = new BrowserFragment();
        fragment.setArguments(args);
        return fragment;
    }

    /**
     * Returns remote counterpart for the BrowserFragment: an {@link IBrowserFragment}.
     */
    /* package */ IBrowserFragment connectFragment(
            IRemoteFragmentClient remoteFragmentClient, Bundle fragmentArgs) {
        try {
            return mImpl.createBrowserFragmentImpl(remoteFragmentClient,
                    ObjectWrapper.wrap(fragmentArgs));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /* package */ ProfileManager getProfileManager() {
        return mProfileManager;
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

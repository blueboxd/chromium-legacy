// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.content.FileProvider;
import android.util.SparseArray;
import android.webkit.ValueCallback;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.ui.base.ResourceBundle;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.ICrashReporterController;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.metrics.UmaUtils;

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/**
 * Root implementation class for WebLayer.
 */
@JNINamespace("weblayer")
public final class WebLayerImpl extends IWebLayer.Stub {
    // TODO: should there be one tag for all this code?
    private static final String TAG = "WebLayer";
    private static final String PRIVATE_DIRECTORY_SUFFIX = "weblayer";
    // TODO: Configure this from the client.
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/weblayer-command-line";

    private final ProfileManager mProfileManager = new ProfileManager();

    private boolean mInited;

    private static class FileProviderHelper implements ContentUriUtils.FileProviderUtil {
        // Keep this variable in sync with the value defined in AndroidManifest.xml.
        private static final String API_AUTHORITY_SUFFIX =
                ".org.chromium.weblayer.client.FileProvider";

        @Override
        public Uri getContentUriFromFile(File file) {
            Context appContext = ContextUtils.getApplicationContext();
            return FileProvider.getUriForFile(
                    appContext, appContext.getPackageName() + API_AUTHORITY_SUFFIX, file);
        }
    }

    WebLayerImpl() {}

    /**
     * Performs the minimal initialization needed for a context. This is used for example in
     * CrashReporterControllerImpl, so it can be used before full WebLayer initialization.
     */
    public static Context minimalInitForContext(IObjectWrapper appContextWrapper) {
        if (ContextUtils.getApplicationContext() != null) {
            return ContextUtils.getApplicationContext();
        }
        // Wrap the app context so that it can be used to load WebLayer implementation classes.
        Context appContext = ClassLoaderContextWrapperFactory.get(
                ObjectWrapper.unwrap(appContextWrapper, Context.class));
        ContextUtils.initApplicationContext(appContext);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DIRECTORY_SUFFIX, PRIVATE_DIRECTORY_SUFFIX);
        return appContext;
    }

    @Override
    public void loadAsync(IObjectWrapper appContextWrapper, IObjectWrapper loadedCallbackWrapper) {
        StrictModeWorkaround.apply();
        init(appContextWrapper);

        final ValueCallback<Boolean> loadedCallback = (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                loadedCallbackWrapper, ValueCallback.class);
        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesAsync(/* startGpu */ false,
                        /* startServiceManagerOnly */ false,
                        new BrowserStartupController.StartupCallback() {
                            @Override
                            public void onSuccess() {
                                CrashReporterControllerImpl.getInstance(appContextWrapper)
                                        .notifyNativeInitialized();
                                loadedCallback.onReceiveValue(true);
                            }
                            @Override
                            public void onFailure() {
                                loadedCallback.onReceiveValue(false);
                            }
                        });
    }

    @Override
    public void loadSync(IObjectWrapper appContextWrapper) {
        StrictModeWorkaround.apply();
        init(appContextWrapper);

        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesSync(
                        /* singleProcess*/ false);
        CrashReporterControllerImpl.getInstance(appContextWrapper).notifyNativeInitialized();
    }

    private void init(IObjectWrapper appContextWrapper) {
        if (mInited) {
            return;
        }
        mInited = true;

        UmaUtils.recordMainEntryPointTime();

        Context appContext = minimalInitForContext(appContextWrapper);
        PackageInfo packageInfo = WebViewFactory.getLoadedPackageInfo();

        // TODO: This can break some functionality of apps that are doing interesting things with
        // Contexts, ideally we would find a better way to do this.
        addWebViewAssetPath(appContext, packageInfo);

        applySplitApkWorkaround(packageInfo.applicationInfo, appContext.getResources().getAssets());
        BuildInfo.setBrowserPackageInfo(packageInfo);
        int resourcesPackageId = getPackageId(appContext, packageInfo.packageName);
        // TODO: The call to onResourcesLoaded() can be slow, we may need to parallelize this with
        // other expensive startup tasks.
        R.onResourcesLoaded(resourcesPackageId);

        ResourceBundle.setAvailablePakLocales(new String[] {}, ProductConfig.UNCOMPRESSED_LOCALES);

        ChildProcessCreationParams.set(appContext.getPackageName(), false /* isExternalService */,
                LibraryProcessType.PROCESS_WEBLAYER_CHILD, true /* bindToCaller */,
                false /* ignoreVisibilityForImportance */,
                "org.chromium.weblayer.ChildProcessService$Privileged",
                "org.chromium.weblayer.ChildProcessService$Sandboxed");

        if (!CommandLine.isInitialized()) {
            // This disk read in the critical path is for development purposes only.
            // TODO: Move it to debug-only (similar to WebView), or allow clients to configure the
            // command line.
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                CommandLine.initFromFile(COMMAND_LINE_FILE);
            }
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());

        // TODO: Validate that doing this disk IO on the main thread is necessary.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_WEBLAYER);
        }
        GmsBridge.getInstance().setSafeBrowsingHandler();
    }

    @Override
    public IBrowserFragment createBrowserFragmentImpl(
            IRemoteFragmentClient fragmentClient, IObjectWrapper fragmentArgs) {
        StrictModeWorkaround.apply();
        Bundle unwrappedArgs = ObjectWrapper.unwrap(fragmentArgs, Bundle.class);
        BrowserFragmentImpl fragment =
                new BrowserFragmentImpl(mProfileManager, fragmentClient, unwrappedArgs);
        return fragment.asIBrowserFragment();
    }

    @Override
    public IProfile getProfile(String profileName) {
        StrictModeWorkaround.apply();
        return mProfileManager.getProfile(profileName);
    }

    @Override
    public void setRemoteDebuggingEnabled(boolean enabled) {
        WebLayerImplJni.get().setRemoteDebuggingEnabled(enabled);
    }

    @Override
    public boolean isRemoteDebuggingEnabled() {
        return WebLayerImplJni.get().isRemoteDebuggingEnabled();
    }

    @Override
    public ICrashReporterController getCrashReporterController(IObjectWrapper appContext) {
        return CrashReporterControllerImpl.getInstance(appContext);
    }

    private static void addWebViewAssetPath(Context appContext, PackageInfo packageInfo) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
                Constructor constructor = WebViewDelegate.class.getDeclaredConstructor();
                constructor.setAccessible(true);
                WebViewDelegate delegate = (WebViewDelegate) constructor.newInstance();
                delegate.addWebViewAssetPath(appContext);
            } else {
                // In L WebViewDelegate did not yet exist, so we have to poke AssetManager directly.
                // Note: like the implementation in WebView's Api21CompatibilityDelegate this does
                // not support split APKs.
                Method addAssetPath = AssetManager.class.getMethod("addAssetPath", String.class);
                addAssetPath.invoke(appContext.getResources().getAssets(),
                        packageInfo.applicationInfo.sourceDir);
            }
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Returns the package ID to use when calling R.onResourcesLoaded().
     */
    private static int getPackageId(Context appContext, String implPackageName) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
                Constructor constructor = WebViewDelegate.class.getDeclaredConstructor();
                constructor.setAccessible(true);
                WebViewDelegate delegate = (WebViewDelegate) constructor.newInstance();
                return delegate.getPackageId(appContext.getResources(), implPackageName);
            } else {
                // In L WebViewDelegate did not yet exist, so we have to look inside AssetManager.
                Method getAssignedPackageIdentifiers =
                        AssetManager.class.getMethod("getAssignedPackageIdentifiers");
                SparseArray packageIdentifiers = (SparseArray) getAssignedPackageIdentifiers.invoke(
                        appContext.getResources().getAssets());
                for (int i = 0; i < packageIdentifiers.size(); i++) {
                    final String name = (String) packageIdentifiers.valueAt(i);

                    if (implPackageName.equals(name)) {
                        return packageIdentifiers.keyAt(i);
                    }
                }
                throw new RuntimeException("Package not found: " + implPackageName);
            }
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    /** Adds assets from split APKs on Android versions where this is broken. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static void applySplitApkWorkaround(
            ApplicationInfo applicationInfo, AssetManager assetManager) {
        // Q already handles this correctly.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return;
        }

        if (applicationInfo.splitSourceDirs != null) {
            try {
                Method addAssetPath;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    addAssetPath = AssetManager.class.getMethod(
                            "addAssetPathAsSharedLibrary", String.class);
                } else {
                    addAssetPath = AssetManager.class.getMethod("addAssetPath", String.class);
                }
                for (String path : applicationInfo.splitSourceDirs) {
                    addAssetPath.invoke(assetManager, path);
                }
            } catch (ReflectiveOperationException e) {
                Log.e(TAG, "Unable to load assets from split APK.", e);
            }
        }
    }

    @NativeMethods
    interface Natives {
        void setRemoteDebuggingEnabled(boolean enabled);
        boolean isRemoteDebuggingEnabled();
    }
}

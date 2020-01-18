// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.content.FileProvider;
import android.util.AndroidRuntimeException;
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
import org.chromium.net.NetworkChangeNotifier;
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
    // Command line flags are only read in debug builds.
    // WARNING: this file is written to by testing code in chrome (see
    // "//chrome/test/chromedriver/chrome/device_manager.cc"). If you change this variable, update
    // "device_manager.cc" too. If the command line file exists in the app's private files
    // directory, it will be read from there, otherwise it will be read from
    // PUBLIC_COMMAND_LINE_FILE.
    private static final String COMMAND_LINE_FILE = "weblayer-command-line";
    private static final String PUBLIC_COMMAND_LINE_FILE = "/data/local/tmp/" + COMMAND_LINE_FILE;
    // This metadata key, if defined, overrides the default behaviour of loading WebLayer from the
    // current WebView implementation. This is only intended for testing, and does not enforce any
    // signature requirements on the implementation, nor does it use the production code path to
    // load the code. Do not set this in production APKs!
    private static final String PACKAGE_MANIFEST_KEY = "org.chromium.weblayer.WebLayerPackage";

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

    @Override
    public void loadAsyncV80(
            IObjectWrapper appContextWrapper, IObjectWrapper loadedCallbackWrapper) {
        loadAsync(appContextWrapper, null, loadedCallbackWrapper);
    }

    @Override
    public void loadAsync(IObjectWrapper appContextWrapper, IObjectWrapper remoteContextWrapper,
            IObjectWrapper loadedCallbackWrapper) {
        StrictModeWorkaround.apply();
        init(appContextWrapper, remoteContextWrapper);

        NetworkChangeNotifier.init();
        WebLayerNetworkChangeNotifierRegistrationPolicy registrationPolicy =
                new WebLayerNetworkChangeNotifierRegistrationPolicy();
        final ValueCallback<Boolean> loadedCallback = (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                loadedCallbackWrapper, ValueCallback.class);
        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesAsync(/* startGpu */ false,
                        /* startServiceManagerOnly */ false,
                        new BrowserStartupController.StartupCallback() {
                            @Override
                            public void onSuccess() {
                                CrashReporterControllerImpl.getInstance().notifyNativeInitialized();
                                configureNetworkChangeNotifier(registrationPolicy);
                                loadedCallback.onReceiveValue(true);
                            }
                            @Override
                            public void onFailure() {
                                loadedCallback.onReceiveValue(false);
                            }
                        });
    }

    @Override
    public void loadSyncV80(IObjectWrapper appContextWrapper) {
        loadSync(appContextWrapper, null);
    }

    @Override
    public void loadSync(IObjectWrapper appContextWrapper, IObjectWrapper remoteContextWrapper) {
        StrictModeWorkaround.apply();
        init(appContextWrapper, remoteContextWrapper);

        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesSync(
                        /* singleProcess*/ false);
        CrashReporterControllerImpl.getInstance().notifyNativeInitialized();
        NetworkChangeNotifier.init();
        configureNetworkChangeNotifier(new WebLayerNetworkChangeNotifierRegistrationPolicy());
    }

    // Configure NetworkChangeNotifier to auto detect changes in network
    // connectivity.
    private void configureNetworkChangeNotifier(
            WebLayerNetworkChangeNotifierRegistrationPolicy registrationPolicy) {
        NetworkChangeNotifier.setAutoDetectConnectivityState(registrationPolicy);
    }

    private void init(IObjectWrapper appContextWrapper, IObjectWrapper remoteContextWrapper) {
        if (mInited) {
            return;
        }
        mInited = true;

        UmaUtils.recordMainEntryPointTime();

        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_WEBLAYER);

        Context appContext = minimalInitForContext(appContextWrapper, remoteContextWrapper);
        PackageInfo packageInfo = WebViewFactory.getLoadedPackageInfo();

        // If a remote context is not provided, the client is an older version that loads the native
        // library on the client side.
        if (remoteContextWrapper != null) {
            loadNativeLibrary(packageInfo.packageName);
        }

        BuildInfo.setBrowserPackageInfo(packageInfo);
        int resourcesPackageId = getPackageId(appContext, packageInfo.packageName);
        // TODO: The call to onResourcesLoaded() can be slow, we may need to parallelize this with
        // other expensive startup tasks.
        R.onResourcesLoaded(resourcesPackageId);

        ResourceBundle.setAvailablePakLocales(new String[] {}, ProductConfig.UNCOMPRESSED_LOCALES);

        setChildProcessCreationParams(appContext, packageInfo.packageName);

        if (!CommandLine.isInitialized()) {
            if (BuildInfo.isDebugAndroid()) {
                // This disk read in the critical path is for development purposes only.
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    File localCommandLineFile =
                            new File(appContext.getFilesDir(), COMMAND_LINE_FILE);
                    if (localCommandLineFile.exists()) {
                        CommandLine.initFromFile(localCommandLineFile.getPath());
                    } else {
                        CommandLine.initFromFile(PUBLIC_COMMAND_LINE_FILE);
                    }
                }
            } else {
                CommandLine.init(null);
            }
        }

        // Creating the Android shared preferences object causes I/O. Prewarm during
        // initialization to avoid this occurring randomly later.
        // TODO: Do this on a background thread.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            ContextUtils.getAppSharedPreferences();
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());

        // TODO: Validate that doing this disk IO on the main thread is necessary.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            LibraryLoader.getInstance().ensureInitialized();
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
        StrictModeWorkaround.apply();
        WebLayerImplJni.get().setRemoteDebuggingEnabled(enabled);
    }

    @Override
    public boolean isRemoteDebuggingEnabled() {
        StrictModeWorkaround.apply();
        return WebLayerImplJni.get().isRemoteDebuggingEnabled();
    }

    @Override
    public ICrashReporterController getCrashReporterControllerV80(IObjectWrapper appContext) {
        StrictModeWorkaround.apply();
        return getCrashReporterController(appContext, null);
    }

    @Override
    public ICrashReporterController getCrashReporterController(
            IObjectWrapper appContext, IObjectWrapper remoteContext) {
        StrictModeWorkaround.apply();
        // This is a no-op if init has already happened.
        WebLayerImpl.minimalInitForContext(appContext, remoteContext);
        return CrashReporterControllerImpl.getInstance();
    }

    /**
     * Creates a remote context. This should only be used for backwards compatibility when the
     * client was not sending the remote context.
     */
    public static Context createRemoteContextV80(Context appContext) {
        try {
            return appContext.createPackageContext(
                    WebViewFactory.getLoadedPackageInfo().packageName,
                    Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (PackageManager.NameNotFoundException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    /**
     * Performs the minimal initialization needed for a context. This is used for example in
     * CrashReporterControllerImpl, so it can be used before full WebLayer initialization.
     */
    private static Context minimalInitForContext(
            IObjectWrapper appContextWrapper, IObjectWrapper remoteContextWrapper) {
        if (ContextUtils.getApplicationContext() != null) {
            return ContextUtils.getApplicationContext();
        }
        Context appContext = ObjectWrapper.unwrap(appContextWrapper, Context.class);
        Context remoteContext = ObjectWrapper.unwrap(remoteContextWrapper, Context.class);
        if (remoteContext == null) {
            remoteContext = createRemoteContextV80(appContext);
        }
        ClassLoaderContextWrapperFactory.setResourceOverrideContext(remoteContext);
        // Wrap the app context so that it can be used to load WebLayer implementation classes.
        appContext = ClassLoaderContextWrapperFactory.get(appContext);
        ContextUtils.initApplicationContext(appContext);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DIRECTORY_SUFFIX, PRIVATE_DIRECTORY_SUFFIX);
        return appContext;
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

    private void loadNativeLibrary(String packageName) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            WebViewFactory.loadWebViewNativeLibraryFromPackage(
                    packageName, getClass().getClassLoader());
        } else {
            try {
                Method loadNativeLibrary =
                        WebViewFactory.class.getDeclaredMethod("loadNativeLibrary");
                loadNativeLibrary.setAccessible(true);
                loadNativeLibrary.invoke(null);
            } catch (ReflectiveOperationException e) {
                Log.e(TAG, "Failed to load native library.", e);
            }
        }
    }

    private void setChildProcessCreationParams(Context appContext, String implPackageName) {
        final boolean bindToCaller = true;
        final boolean ignoreVisibilityForImportance = false;
        final String privilegedServicesPackageName = appContext.getPackageName();
        final String privilegedServicesName =
                "org.chromium.weblayer.ChildProcessService$Privileged";

        String sandboxedServicesPackageName = appContext.getPackageName();
        String sandboxedServicesName = "org.chromium.weblayer.ChildProcessService$Sandboxed";
        boolean isExternalService = false;
        boolean loadedFromWebView = wasLoadedFromWebView(appContext);
        if (loadedFromWebView && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // On O+ when loading from a WebView implementation, we can just use WebView's declared
            // external services as our renderers, which means we benefit from the webview zygote
            // process. We still need to use the client's privileged services, as only isolated
            // services can be external.
            isExternalService = true;
            sandboxedServicesPackageName = implPackageName;
            sandboxedServicesName = null;
        }

        ChildProcessCreationParams.set(privilegedServicesPackageName, privilegedServicesName,
                sandboxedServicesPackageName, sandboxedServicesName, isExternalService,
                LibraryProcessType.PROCESS_WEBLAYER_CHILD, bindToCaller,
                ignoreVisibilityForImportance);
    }

    private static boolean wasLoadedFromWebView(Context appContext) {
        try {
            Bundle metaData = appContext.getPackageManager()
                                      .getApplicationInfo(appContext.getPackageName(),
                                              PackageManager.GET_META_DATA)
                                      .metaData;
            if (metaData != null && metaData.getString(PACKAGE_MANIFEST_KEY) != null) {
                return false;
            }
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            // This would indicate the client app doesn't exist;
            // just return true as there's nothing sensible to do here.
            return true;
        }
    }

    @NativeMethods
    interface Natives {
        void setRemoteDebuggingEnabled(boolean enabled);
        boolean isRemoteDebuggingEnabled();
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.StrictMode;
import android.text.TextUtils;
import android.util.Pair;

import dalvik.system.BaseDexClassLoader;
import dalvik.system.PathClassLoader;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/** Helper class which performs initialization needed for WebView compatibility. */
final class WebViewCompatibilityHelper {
    /** Creates a the ClassLoader to use for WebView compatibility. */
    static Pair<Callable<ClassLoader>, WebLayer.WebViewCompatibilityResult> initialize(
            Context appContext)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        Context remoteContext = WebLayer.getOrCreateRemoteContext(appContext);
        PackageInfo info =
                appContext.getPackageManager().getPackageInfo(remoteContext.getPackageName(),
                        PackageManager.GET_SHARED_LIBRARY_FILES
                                | PackageManager.MATCH_UNINSTALLED_PACKAGES);
        int majorVersion = parseMajorVersion(info.versionName);
        if (!isSupportedVersion(majorVersion)) {
            return Pair.create(
                    null, WebLayer.WebViewCompatibilityResult.FAILURE_UNSUPPORTED_VERSION);
        }
        if (majorVersion >= 84) {
            // Recreate the context without code to avoid wasting memory by accidentally using the
            // class loader.
            remoteContext = appContext.createPackageContext(
                    remoteContext.getPackageName(), Context.CONTEXT_IGNORE_SECURITY);
            WebLayer.setRemoteContext(remoteContext);
        }
        // Prepend "/." to all library paths. This changes the library path while still pointing to
        // the same directory, allowing us to get around a check in the JVM. This is only necessary
        // for N+, where we rely on linker namespaces.
        String[] libraryPaths;
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            // Even if the context was recreated without code above, on N+ the library path list
            // still contains the necessary paths to load WebLayer.
            libraryPaths = getLibraryPaths(remoteContext.getClassLoader());
            for (int i = 0; i < libraryPaths.length; i++) {
                assert libraryPaths[i].startsWith("/");
                libraryPaths[i] = "/." + libraryPaths[i];
            }
        } else {
            // Android M- only need the native lib dir, since standalone WebView stores native libs
            // compressed and they get extracted here. Standalone WebView also doesn't depend on any
            // shared library APKs like Trichrome WebView.
            libraryPaths = new String[] {info.applicationInfo.nativeLibraryDir};
        }

        String dexPath = getAllApkPaths(info.applicationInfo);
        String librarySearchPath = TextUtils.join(File.pathSeparator, libraryPaths);
        Callable<ClassLoader> classLoaderGetter = () -> {
            // TODO(cduvall): PathClassLoader may call stat on the library paths, consider moving
            // this to a background thread.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                return new PathClassLoader(
                        dexPath, librarySearchPath, ClassLoader.getSystemClassLoader());
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        };
        return Pair.create(classLoaderGetter, WebLayer.WebViewCompatibilityResult.SUCCESS);
    }

    /**
     * Returns if the version of the WebLayer implementation supports WebView compatibility. We
     * can't use WebLayer.getSupportedMajorVersion() here because the loader depends on
     * WebView compatibility already being set up.
     */
    static boolean isSupportedVersion(int majorVersion) {
        // M- only supports WebView compat via copying the libraries on 81, so only support 82+.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            return majorVersion >= 82;
        }
        return majorVersion >= 81;
    }

    /** Parses the version name into an integer version number. */
    static int parseMajorVersion(String versionName) {
        if (versionName == null) {
            return -1;
        }
        String[] parts = versionName.split("\\.", -1);
        if (parts.length < 4) {
            return -1;
        }
        try {
            return Integer.parseInt(parts[0]);
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /** Returns the library paths the given class loader will search. */
    static String[] getLibraryPaths(ClassLoader classLoader) throws ReflectiveOperationException {
        // This seems to be the best way to reliably get both the native lib directory and the path
        // within the APK where libs might be stored.
        return ((String) BaseDexClassLoader.class.getDeclaredMethod("getLdLibraryPath")
                        .invoke((BaseDexClassLoader) classLoader))
                .split(":");
    }

    /** This is mostly taken from ApplicationInfo.getAllApkPaths(). */
    private static String getAllApkPaths(ApplicationInfo info) {
        // The OS version of this method also includes resourceDirs, but this is not available in
        // the SDK.
        final String[][] inputLists = {info.sharedLibraryFiles, info.splitSourceDirs};
        final List<String> output = new ArrayList<>(10);
        for (String[] inputList : inputLists) {
            if (inputList != null) {
                for (String input : inputList) {
                    output.add(input);
                }
            }
        }
        if (info.sourceDir != null) {
            output.add(info.sourceDir);
        }
        return TextUtils.join(File.pathSeparator, output);
    }

    private WebViewCompatibilityHelper() {}
}

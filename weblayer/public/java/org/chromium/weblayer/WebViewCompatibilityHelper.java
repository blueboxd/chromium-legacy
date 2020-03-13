// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.system.ErrnoException;
import android.system.Os;
import android.text.TextUtils;

import dalvik.system.BaseDexClassLoader;
import dalvik.system.DexClassLoader;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Helper class which performs initialization needed for WebView compatibility. */
@SuppressWarnings("NoAndroidAsyncTaskCheck")
final class WebViewCompatibilityHelper {
    private final PackageInfo mPackageInfo;
    private final GetClassLoaderPathsTask mClassLoaderPathsTask;

    WebViewCompatibilityHelper(String[] libraryPaths, PackageInfo packageInfo, File baseDir,
            Callback<WebLayer.WebViewCompatibilityResult> callback) {
        mPackageInfo = packageInfo;
        mClassLoaderPathsTask =
                new GetClassLoaderPathsTask(libraryPaths, mPackageInfo, baseDir, callback);
        mClassLoaderPathsTask.executeOnExecutor(android.os.AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Creates a WebViewCompatibilityHelper and begins initialization. */
    static WebViewCompatibilityHelper initialize(Context appContext, Context remoteContext,
            File baseDir, Callback<WebLayer.WebViewCompatibilityResult> callback)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        PackageInfo info =
                appContext.getPackageManager().getPackageInfo(remoteContext.getPackageName(),
                        PackageManager.GET_SHARED_LIBRARY_FILES
                                | PackageManager.MATCH_UNINSTALLED_PACKAGES);
        if (!isSupportedVersion(info.versionName)) {
            if (callback != null) {
                callback.onResult(WebLayer.WebViewCompatibilityResult.FAILURE_UNSUPPORTED_VERSION);
            }
            return null;
        }
        return new WebViewCompatibilityHelper(
                getLibraryPaths(remoteContext.getClassLoader()), info, baseDir, callback);
    }

    /**
     * Returns if the version of the WebLayer implementation supports WebView compatibility. We
     * can't use WebLayer.getSupportedMajorVersion() here because the loader depends on
     * WebView compatibility already being set up.
     */
    static boolean isSupportedVersion(String versionName) {
        if (versionName == null) {
            return false;
        }
        String[] parts = versionName.split("\\.", -1);
        if (parts.length < 4) {
            return false;
        }
        int majorVersion = 0;
        try {
            majorVersion = Integer.parseInt(parts[0]);
        } catch (NumberFormatException e) {
            return false;
        }
        return majorVersion >= 81;
    }

    /**
     * Returns the class loader to be used for WebView compatibility. This class loader will not
     * interfere with any WebViews that are created.
     */
    ClassLoader getWebLayerClassLoader() throws ExecutionException, InterruptedException {
        ClassLoaderPaths paths = mClassLoaderPathsTask.get();
        if (paths.mResult != WebLayer.WebViewCompatibilityResult.SUCCESS_CACHED
                && paths.mResult != WebLayer.WebViewCompatibilityResult.SUCCESS_COPIED) {
            return null;
        }
        return new DexClassLoader(getAllApkPaths(mPackageInfo.applicationInfo),
                paths.mOptimizedDirectory, paths.mLibrarySearchPath,
                ClassLoader.getSystemClassLoader());
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
    private String getAllApkPaths(ApplicationInfo info) {
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

    private class ClassLoaderPaths {
        String mLibrarySearchPath;
        String mOptimizedDirectory;
        WebLayer.WebViewCompatibilityResult mResult =
                WebLayer.WebViewCompatibilityResult.SUCCESS_CACHED;
    }

    /**
     * Task to retrieve file paths which should be used when creating a class loader compatible
     * with WebView.
     */
    private class GetClassLoaderPathsTask
            extends android.os.AsyncTask<Void, Void, ClassLoaderPaths> {
        private static final String PRIVATE_DIR = "weblayer_private";

        private final String[] mOriginalLibraryPaths;
        private final PackageInfo mPackageInfo;
        private final File mBaseDir;
        private final Callback<WebLayer.WebViewCompatibilityResult> mCallback;

        public GetClassLoaderPathsTask(String[] libraryPaths, PackageInfo packageInfo, File baseDir,
                Callback<WebLayer.WebViewCompatibilityResult> callback) {
            mOriginalLibraryPaths = libraryPaths;
            mPackageInfo = packageInfo;
            mBaseDir = baseDir;
            mCallback = callback;
        }

        @Override
        protected ClassLoaderPaths doInBackground(Void... params) {
            File webLayerDir = getWebLayerDir();
            ClassLoaderPaths paths = new ClassLoaderPaths();
            try {
                addLibrarySearchPath(webLayerDir, paths);
            } catch (ErrnoException | IOException e) {
                // Make sure to clean up anything left over after an IOException.
                recursivelyDeleteFile(new File(mBaseDir, PRIVATE_DIR));

                paths.mResult = WebLayer.WebViewCompatibilityResult.FAILURE_IO_ERROR;
                return paths;
            }
            File codeCacheDir = new File(webLayerDir, "codecache");
            codeCacheDir.mkdirs();
            paths.mOptimizedDirectory = codeCacheDir.toString();
            return paths;
        }

        @Override
        protected void onPostExecute(ClassLoaderPaths paths) {
            if (mCallback != null) {
                mCallback.onResult(paths.mResult);
            }
        }

        /**
         * Creates the directory to copy/symlink WebView libraries. This will be a directory
         * structure like files/weblayer_private/374100010/.
         */
        private File getWebLayerDir() {
            File webLayerDir = new File(mBaseDir, PRIVATE_DIR);
            webLayerDir.mkdirs();
            // Clean up previous versions before we copy any files.
            String[] names = webLayerDir.list();
            String versionCodeString = String.valueOf(mPackageInfo.versionCode);
            if (names != null) {
                for (String name : names) {
                    if (!name.equals(versionCodeString)) {
                        recursivelyDeleteFile(new File(webLayerDir, name));
                    }
                }
            }

            File finalDir = new File(webLayerDir, versionCodeString);
            finalDir.mkdirs();
            return finalDir;
        }

        private void addLibrarySearchPath(File webLayerDir, ClassLoaderPaths paths)
                throws ErrnoException, IOException {
            ArrayList<String> finalPaths = new ArrayList<>();
            for (int i = 0; i < mOriginalLibraryPaths.length; i++) {
                File path = new File(mOriginalLibraryPaths[i]);
                // Skip libs from other directories like /vendor.
                if (!path.toString().startsWith("/data")) {
                    continue;
                }
                File newPath = new File(webLayerDir, "lib" + i);
                String suffix = "";
                if (path.toString().contains("!")) {
                    String[] splitPath = path.toString().split("!");
                    path = new File(splitPath[0]);
                    suffix = "!" + splitPath[1];
                }
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
                    // If this path isn't a directory, it's probably pointing to the APK. We don't
                    // want to try to copy the APK, and libraries are only stored in the APK in
                    // Monochrome/Trichrome which is only used for N+.
                    if (!path.isDirectory()) {
                        continue;
                    }
                    for (String libName : path.list()) {
                        // Make sure we only copy the necessary lib.
                        if (!libName.equals("libwebviewchromium.so")) {
                            continue;
                        }
                        File oldFile = new File(path, libName);
                        File newFile = new File(newPath, libName.replace(".so", "-weblayer.so"));
                        if (!newFile.exists()) {
                            newPath.mkdirs();
                            copyFile(oldFile, newFile);
                            paths.mResult = WebLayer.WebViewCompatibilityResult.SUCCESS_COPIED;
                        }
                    }
                } else {
                    newPath.mkdirs();
                    // Make sure to re-create the symlink in case the WebView APK moved.
                    newPath.delete();
                    Os.symlink(path.toString(), newPath.toString());
                }
                finalPaths.add(newPath.toString() + suffix);
            }
            paths.mLibrarySearchPath = TextUtils.join(File.pathSeparator, finalPaths);
        }

        private void copyFile(File src, File dst) throws IOException {
            try (FileInputStream inputStream = new FileInputStream(src);
                    FileOutputStream outputStream = new FileOutputStream(dst)) {
                byte[] bytes = new byte[8192];
                int length;
                while ((length = inputStream.read(bytes)) > 0) {
                    outputStream.write(bytes, 0, length);
                }
            }
        }

        private void recursivelyDeleteFile(File currentFile) {
            if (!currentFile.exists()) {
                // This file could be a broken symlink, so try to delete. If we don't delete a
                // broken symlink, the directory containing it cannot be deleted.
                currentFile.delete();
                return;
            }
            if (currentFile.isDirectory()) {
                File[] files = currentFile.listFiles();
                if (files != null) {
                    for (File file : files) {
                        recursivelyDeleteFile(file);
                    }
                }
            }

            currentFile.delete();
        }
    }
}

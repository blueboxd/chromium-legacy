// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import dalvik.system.DexClassLoader;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FileUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.TestFileUtil;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for (@link WebViewCompatibilityHelper}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewCompatibilityHelperTest {
    private File mTmpDir;
    private File mOriginalLibDir;
    private File mNewLibDir;
    private PackageInfo mPackageInfo;

    private static class ResultHelper implements Callback<WebLayer.WebViewCompatibilityResult> {
        private WebLayer.WebViewCompatibilityResult mResult;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void onResult(WebLayer.WebViewCompatibilityResult result) {
            mResult = result;
            mCallbackHelper.notifyCalled();
        }

        public WebLayer.WebViewCompatibilityResult getResult() throws TimeoutException {
            mCallbackHelper.waitForFirst();
            return mResult;
        }
    }

    @Before
    public void setUp() {
        mTmpDir = InstrumentationRegistry.getTargetContext().getCacheDir();
        mOriginalLibDir = new File(mTmpDir, "original");
        mOriginalLibDir.mkdirs();

        mNewLibDir = new File(mTmpDir, "new");

        mPackageInfo = new PackageInfo();
        mPackageInfo.versionCode = 1;
        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.publicSourceDir = new File(mTmpDir, "fake.apk").toString();
        mPackageInfo.applicationInfo = applicationInfo;
    }

    @Test
    @SmallTest
    public void testLibsCopied() throws Exception {
        writeFile(new File(mOriginalLibDir, "libwebviewchromium.so"), "foo");
        writeFile(new File(mOriginalLibDir, "libbar.so"), "bar");
        String[] libraryPaths = new String[] {mOriginalLibDir.toString()};
        ResultHelper helper = new ResultHelper();
        DexClassLoader classLoader = (DexClassLoader) new WebViewCompatibilityHelper(
                libraryPaths, mPackageInfo, mNewLibDir, helper)
                                             .getWebLayerClassLoader();
        Assert.assertEquals(helper.getResult(), getExpectedCopyResult());

        File libDir = new File(mNewLibDir, "weblayer_private/1/lib0");
        File foo = new File(libDir, getExpectedLibFileName());
        Assert.assertEquals(readFile(foo), "foo");
        Assert.assertEquals(classLoader.findLibrary(getExpectedLibName()), foo.toString());
        // M- will only copy libwebviewchromium.so.
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            File bar = new File(libDir, "libbar.so");
            Assert.assertEquals(readFile(bar), "bar");
            Assert.assertEquals(classLoader.findLibrary("bar"), bar.toString());
        }

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            Assert.assertFalse(isSymlink(libDir));
        } else {
            Assert.assertTrue(isSymlink(libDir));
        }
    }

    @Test
    @SmallTest
    public void testIoError() throws Exception {
        writeFile(new File(mOriginalLibDir, "libwebviewchromium.so"), "foo");
        String[] libraryPaths = new String[] {mOriginalLibDir.toString()};
        ResultHelper helper = new ResultHelper();
        DexClassLoader classLoader = (DexClassLoader) new WebViewCompatibilityHelper(
                libraryPaths, mPackageInfo, new File("/bad_dir"), helper)
                                             .getWebLayerClassLoader();
        Assert.assertNull(classLoader);
        Assert.assertEquals(
                helper.getResult(), WebLayer.WebViewCompatibilityResult.FAILURE_IO_ERROR);
    }

    @Test
    @SmallTest
    public void testMultipleLibDirs() throws Exception {
        writeFile(new File(mOriginalLibDir, "foo/foo"), "foo");
        writeFile(new File(mOriginalLibDir, "bar/libwebviewchromium.so"), "bar");
        String[] libraryPaths = new String[] {new File(mOriginalLibDir, "foo").toString(),
                new File(mOriginalLibDir, "bar").toString()};
        ResultHelper helper = new ResultHelper();
        new WebViewCompatibilityHelper(libraryPaths, mPackageInfo, mNewLibDir, helper)
                .getWebLayerClassLoader();
        Assert.assertEquals(helper.getResult(), getExpectedCopyResult());

        // M- will only copy libwebviewchromium.so.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            Assert.assertFalse(new File(mNewLibDir, "weblayer_private/1/lib0/").exists());
            Assert.assertTrue(
                    new File(mNewLibDir, "weblayer_private/1/lib1/" + getExpectedLibFileName())
                            .exists());
        } else {
            Assert.assertTrue(new File(mNewLibDir, "weblayer_private/1/lib0/foo").exists());
            Assert.assertTrue(
                    new File(mNewLibDir, "weblayer_private/1/lib1/" + getExpectedLibFileName())
                            .exists());
        }
    }

    @Test
    @SmallTest
    public void testLibsNotCopiedAgain() throws Exception {
        writeFile(new File(mOriginalLibDir, "libwebviewchromium.so"), "foo");
        String[] libraryPaths = new String[] {mOriginalLibDir.toString()};
        ResultHelper helper = new ResultHelper();
        new WebViewCompatibilityHelper(libraryPaths, mPackageInfo, mNewLibDir, helper)
                .getWebLayerClassLoader();
        Assert.assertEquals(helper.getResult(), getExpectedCopyResult());

        File foo = new File(mNewLibDir, "weblayer_private/1/lib0/" + getExpectedLibFileName());
        Assert.assertTrue(foo.exists());
        long lastModified = foo.lastModified();

        // Sleep for 1 sec to guarantee last modified would have changed.
        Thread.sleep(1000);

        helper = new ResultHelper();
        new WebViewCompatibilityHelper(libraryPaths, mPackageInfo, mNewLibDir, helper)
                .getWebLayerClassLoader();
        Assert.assertEquals(helper.getResult(), WebLayer.WebViewCompatibilityResult.SUCCESS_CACHED);
        Assert.assertTrue(foo.exists());
        Assert.assertEquals(foo.lastModified(), lastModified);
    }

    @Test
    @SmallTest
    public void testOldLibsDeleted() throws Exception {
        writeFile(new File(mOriginalLibDir, "libwebviewchromium.so"), "foo");
        String[] libraryPaths = new String[] {mOriginalLibDir.toString()};
        ResultHelper helper = new ResultHelper();
        new WebViewCompatibilityHelper(libraryPaths, mPackageInfo, mNewLibDir, helper)
                .getWebLayerClassLoader();
        Assert.assertEquals(helper.getResult(), getExpectedCopyResult());

        File oldFile = new File(mNewLibDir, "weblayer_private/1/lib0/" + getExpectedLibFileName());
        Assert.assertTrue(oldFile.exists());

        FileUtils.recursivelyDeleteFile(mOriginalLibDir, FileUtils.DELETE_ALL);

        File originalLibDir2 = new File(mTmpDir, "original2");
        originalLibDir2.mkdirs();
        writeFile(new File(originalLibDir2, "libwebviewchromium.so"), "foo2");
        mPackageInfo.versionCode = 2;
        libraryPaths = new String[] {originalLibDir2.toString()};
        helper = new ResultHelper();
        new WebViewCompatibilityHelper(libraryPaths, mPackageInfo, mNewLibDir, helper)
                .getWebLayerClassLoader();
        Assert.assertEquals(helper.getResult(), getExpectedCopyResult());
        Assert.assertFalse(oldFile.exists());
        Assert.assertFalse(new File(mNewLibDir, "weblayer_private/1").exists());

        Assert.assertEquals(readFile(new File(mNewLibDir,
                                    "weblayer_private/2/lib0/" + getExpectedLibFileName())),
                "foo2");
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testZipPath() throws Exception {
        File originalZip = new File(mOriginalLibDir, "foo.apk");
        writeFile(originalZip, "zip stuff");
        String[] libraryPaths = new String[] {originalZip.toString() + "!/libs"};
        ResultHelper helper = new ResultHelper();
        ClassLoader classLoader =
                new WebViewCompatibilityHelper(libraryPaths, mPackageInfo, mNewLibDir, helper)
                        .getWebLayerClassLoader();
        Assert.assertEquals(helper.getResult(), getExpectedCopyResult());

        File newZip = new File(mNewLibDir, "weblayer_private/1/lib0");
        Assert.assertArrayEquals(WebViewCompatibilityHelper.getLibraryPaths(classLoader),
                new String[] {newZip.toString() + "!/libs"});
        Assert.assertTrue(newZip.exists());
        Assert.assertTrue(isSymlink(newZip));
    }

    @Test
    @SmallTest
    public void testSupportedVersion() throws Exception {
        Assert.assertTrue(WebViewCompatibilityHelper.isSupportedVersion("81.0.2.5"));
        Assert.assertTrue(WebViewCompatibilityHelper.isSupportedVersion("82.0.2.5"));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion("80.0.2.5"));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion(""));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion("82.0"));
        Assert.assertFalse(WebViewCompatibilityHelper.isSupportedVersion(null));
    }

    private void writeFile(File file, String contents) throws Exception {
        file.getParentFile().mkdirs();
        FileUtils.copyStreamToFile(new ByteArrayInputStream(contents.getBytes()), file);
    }

    private String readFile(File file) throws Exception {
        // Assert.assertTrue(file.exists());
        return new String(TestFileUtil.readUtf8File(file.toString(), 1024 /* sizeLimit */));
    }

    private boolean isSymlink(File file) throws IOException {
        File canonical;
        if (file.getParent() == null) {
            canonical = file;
        } else {
            File canonicalDir = file.getParentFile().getCanonicalFile();
            canonical = new File(canonicalDir, file.getName());
        }
        return !canonical.getCanonicalFile().equals(canonical.getAbsoluteFile());
    }

    private static String getExpectedLibName() {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            return "webviewchromium-weblayer";
        }
        return "webviewchromium";
    }

    private static String getExpectedLibFileName() {
        return "lib" + getExpectedLibName() + ".so";
    }

    private static WebLayer.WebViewCompatibilityResult getExpectedCopyResult() {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            return WebLayer.WebViewCompatibilityResult.SUCCESS_COPIED;
        }
        return WebLayer.WebViewCompatibilityResult.SUCCESS_CACHED;
    }
}

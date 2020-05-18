// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.containsInAnyOrder;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;
import static org.chromium.android_webview.test.common.crash.CrashInfoEqualityMatcher.equalsTo;
import static org.chromium.android_webview.test.common.crash.CrashInfoTest.createCrashInfo;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.devui.util.WebViewCrashInfoCollector;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;

import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for WebViewCrashInfoCollector.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class WebViewCrashInfoCollectorTest {
    /**
     * Test that merging {@code CrashInfo} that has the same {@code localID} works correctly.
     */
    @Test
    @SmallTest
    public void testMergeDuplicates() {
        List<CrashInfo> testList = Arrays.asList(
                createCrashInfo("xyz123", 112233445566L, null, -1, null, UploadState.PENDING),
                createCrashInfo(
                        "def789", -1, "55667788", 123344556677L, null, UploadState.UPLOADED),
                createCrashInfo("abc456", -1, null, -1, null, UploadState.PENDING),
                createCrashInfo("xyz123", 112233445566L, null, -1, "com.test.package", null),
                createCrashInfo("abc456", 445566778899L, null, -1, "org.test.package", null),
                createCrashInfo("abc456", -1, null, -1, null, null),
                createCrashInfo(
                        "xyz123", -1, "11223344", 223344556677L, null, UploadState.UPLOADED));
        List<CrashInfo> uniqueList = WebViewCrashInfoCollector.mergeDuplicates(testList);
        Assert.assertThat(uniqueList,
                containsInAnyOrder(equalsTo(createCrashInfo("abc456", 445566778899L, null, -1,
                                           "org.test.package", UploadState.PENDING)),
                        equalsTo(createCrashInfo("xyz123", 112233445566L, "11223344", 223344556677L,
                                "com.test.package", UploadState.UPLOADED)),
                        equalsTo(createCrashInfo("def789", -1, "55667788", 123344556677L, null,
                                UploadState.UPLOADED))));
    }

    /**
     * Test that sort method works correctly; it sorts by recent crashes first (capture time then
     * upload time).
     */
    @Test
    @SmallTest
    public void testSortByRecentCaptureTime() {
        List<CrashInfo> testList = Arrays.asList(
                createCrashInfo("xyz123", -1, "11223344", 123L, null, UploadState.UPLOADED),
                createCrashInfo("def789", 111L, "55667788", 100L, null, UploadState.UPLOADED),
                createCrashInfo("abc456", -1, null, -1, null, UploadState.PENDING),
                createCrashInfo("ghijkl", 112L, null, -1, "com.test.package", null),
                createCrashInfo("abc456", 112L, null, 112L, "org.test.package", null),
                createCrashInfo(null, 100, "11223344", -1, "com.test.package", null),
                createCrashInfo("abc123", 100, null, -1, null, null));
        WebViewCrashInfoCollector.sortByMostRecent(testList);
        Assert.assertThat(testList,
                contains(equalsTo(createCrashInfo(
                                 "abc456", 112L, null, 112L, "org.test.package", null)),
                        equalsTo(createCrashInfo(
                                "ghijkl", 112L, null, -1, "com.test.package", null)),
                        equalsTo(createCrashInfo(
                                "def789", 111L, "55667788", 100L, null, UploadState.UPLOADED)),
                        equalsTo(createCrashInfo(
                                null, 100, "11223344", -1, "com.test.package", null)),
                        equalsTo(createCrashInfo("abc123", 100, null, -1, null, null)),
                        equalsTo(createCrashInfo(
                                "xyz123", -1, "11223344", 123L, null, UploadState.UPLOADED)),
                        equalsTo(createCrashInfo(
                                "abc456", -1, null, -1, null, UploadState.PENDING))));
    }
}

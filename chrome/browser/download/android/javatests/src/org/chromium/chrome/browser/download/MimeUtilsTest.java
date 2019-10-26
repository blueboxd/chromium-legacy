// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for MimeUtils class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class MimeUtilsTest {
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetFileExtension() {
        Assert.assertEquals("ext", MimeUtils.getFileExtension("", "file.ext"));
        Assert.assertEquals("ext", MimeUtils.getFileExtension("http://file.ext", ""));
        Assert.assertEquals("txt", MimeUtils.getFileExtension("http://file.ext", "file.txt"));
        Assert.assertEquals("txt", MimeUtils.getFileExtension("http://file.ext", "file name.txt"));
    }
}

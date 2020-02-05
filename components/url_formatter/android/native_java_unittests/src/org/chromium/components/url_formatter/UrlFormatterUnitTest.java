// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.url_formatter;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;

/**
 * Unit tests for {@link UrlFormatter}.
 *
 * These tests are basic sanity checks to ensure the plumbing is working correctly. The wrapped
 * functions are tested much more thoroughly elsewhere.
 */
public class UrlFormatterUnitTest {
    @CalledByNative
    private UrlFormatterUnitTest() {}

    @CalledByNativeJavaTest
    public void testFixupUrl() {
        Assert.assertEquals("http://google.com/", UrlFormatter.fixupUrl("google.com").getSpec());
        Assert.assertEquals("chrome://version/", UrlFormatter.fixupUrl("about:").getSpec());
        Assert.assertEquals("file:///mail.google.com:/",
                UrlFormatter.fixupUrl("//mail.google.com:/").getSpec());
        Assert.assertFalse(UrlFormatter.fixupUrl("0x100.0").isValid());
    }
}

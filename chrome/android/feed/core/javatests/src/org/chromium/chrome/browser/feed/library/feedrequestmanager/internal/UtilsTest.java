// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager.internal;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.wire.VersionProto.Version;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link Utils}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UtilsTest {
    @Test
    public void fillVersionsFromString_allValid() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "1.2.3.4");
        Assert.assertEquals(1, builder.getMajor());
        Assert.assertEquals(2, builder.getMinor());
        Assert.assertEquals(3, builder.getBuild());
        Assert.assertEquals(4, builder.getRevision());
    }

    @Test
    public void fillVersionsFromString_ignoresExtraStrings() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "1.2.3b5");
        Assert.assertEquals(1, builder.getMajor());
        Assert.assertEquals(2, builder.getMinor());
        Assert.assertEquals(3, builder.getBuild());
        Assert.assertFalse(builder.hasRevision());
    }

    @Test
    public void fillVersionsFromString_emptyVersion() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "");
        Assert.assertFalse(builder.hasMajor());
        Assert.assertFalse(builder.hasMinor());
        Assert.assertFalse(builder.hasBuild());
        Assert.assertFalse(builder.hasRevision());
    }
}

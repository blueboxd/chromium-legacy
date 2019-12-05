// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager.internal;

import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.components.feed.core.proto.wire.VersionProto.Version;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link Utils}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Ignore("TODO(crbug.com/1024945): this test needs rewritten without LiteProtoTruth.")
public class UtilsTest {
    @Test
    public void fillVersionsFromString_allValid() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "1.2.3.4");
        // TODO(crbug.com/1024945): Find alternative to LiteProtoTruth.
        // LiteProtoTruth.assertThat(builder.build())
        //         .isEqualTo(Version.newBuilder()
        //                            .setMajor(1)
        //                            .setMinor(2)
        //                            .setBuild(3)
        //                            .setRevision(4)
        //                            .build());
    }

    @Test
    public void fillVersionsFromString_ignoresExtraStrings() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "1.2.3b5");
        // TODO(crbug.com/1024945): Find alternative to LiteProtoTruth.
        // LiteProtoTruth.assertThat(builder.build())
        //         .isEqualTo(Version.newBuilder().setMajor(1).setMinor(2).setBuild(3).build());
    }

    @Test
    public void fillVersionsFromString_emptyVersion() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "");
        // TODO(crbug.com/1024945): Find alternative to LiteProtoTruth.
        // LiteProtoTruth.assertThat(builder.build()).isEqualToDefaultInstance();
    }
}

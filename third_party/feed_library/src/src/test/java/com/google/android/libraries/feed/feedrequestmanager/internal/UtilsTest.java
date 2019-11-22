// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedrequestmanager.internal;

import com.google.common.truth.extensions.proto.LiteProtoTruth;
import com.google.search.now.wire.feed.VersionProto.Version;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link Utils}. */
@RunWith(RobolectricTestRunner.class)
public class UtilsTest {
    @Test
    public void fillVersionsFromString_allValid() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "1.2.3.4");
        LiteProtoTruth.assertThat(builder.build())
                .isEqualTo(Version.newBuilder()
                                   .setMajor(1)
                                   .setMinor(2)
                                   .setBuild(3)
                                   .setRevision(4)
                                   .build());
    }

    @Test
    public void fillVersionsFromString_ignoresExtraStrings() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "1.2.3b5");
        LiteProtoTruth.assertThat(builder.build())
                .isEqualTo(Version.newBuilder().setMajor(1).setMinor(2).setBuild(3).build());
    }

    @Test
    public void fillVersionsFromString_emptyVersion() {
        Version.Builder builder = Version.newBuilder();
        Utils.fillVersionsFromString(builder, "");
        LiteProtoTruth.assertThat(builder.build()).isEqualToDefaultInstance();
    }
}

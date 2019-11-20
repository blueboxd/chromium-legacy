// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
        .isEqualTo(Version.newBuilder().setMajor(1).setMinor(2).setBuild(3).setRevision(4).build());
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

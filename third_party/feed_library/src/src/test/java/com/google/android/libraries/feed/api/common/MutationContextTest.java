// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.common;

import static com.google.common.truth.Truth.assertThat;

import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.nio.charset.Charset;

/** Tests of the {@link MutationContext} class. */
@RunWith(RobolectricTestRunner.class)
public class MutationContextTest {
    @Test
    public void testBuilder() {
        ByteString tokenBytes = ByteString.copyFrom("token", Charset.defaultCharset());
        StreamToken token = StreamToken.newBuilder().setNextPageToken(tokenBytes).build();
        String sessionId = "session:1";
        MutationContext mutationContext = new MutationContext.Builder()
                                                  .setContinuationToken(token)
                                                  .setRequestingSessionId(sessionId)
                                                  .build();
        assertThat(mutationContext).isNotNull();
        assertThat(mutationContext.getContinuationToken()).isEqualTo(token);
        assertThat(mutationContext.getRequestingSessionId()).isEqualTo(sessionId);
        assertThat(mutationContext.isUserInitiated()).isFalse();
    }

    @Test
    public void testUserInitiated_true() {
        MutationContext mutationContext =
                new MutationContext.Builder().setUserInitiated(true).build();
        assertThat(mutationContext.isUserInitiated()).isTrue();
    }
}

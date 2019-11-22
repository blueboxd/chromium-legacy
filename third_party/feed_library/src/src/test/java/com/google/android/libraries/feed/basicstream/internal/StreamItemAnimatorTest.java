// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import com.google.android.libraries.feed.basicstream.internal.viewholders.FeedViewHolder;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link StreamItemAnimator}. */
@RunWith(RobolectricTestRunner.class)
public class StreamItemAnimatorTest {
    @Mock
    private ContentChangedListener contentChangedListener;
    @Mock
    private FeedViewHolder feedViewHolder;

    private StreamItemAnimatorForTest streamItemAnimator;

    @Before
    public void setup() {
        initMocks(this);
        streamItemAnimator = new StreamItemAnimatorForTest(contentChangedListener);
    }

    @Test
    public void testOnAnimationFinished() {
        streamItemAnimator.onAnimationFinished(feedViewHolder);
        verify(contentChangedListener).onContentChanged();
    }

    @Test
    public void setStreamVisibility_toVisible_endsAnimations() {
        streamItemAnimator.setStreamVisibility(true);
        assertThat(streamItemAnimator.animationsEnded).isTrue();
    }

    private static class StreamItemAnimatorForTest extends StreamItemAnimator {
        private boolean animationsEnded;

        StreamItemAnimatorForTest(ContentChangedListener contentChangedListener) {
            super(contentChangedListener);
        }

        @Override
        public void endAnimations() {
            super.endAnimations();
            animationsEnded = true;
        }
    }
}

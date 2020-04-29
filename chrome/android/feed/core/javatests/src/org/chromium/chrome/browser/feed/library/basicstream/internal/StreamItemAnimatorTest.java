// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ViewActionManager;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link StreamItemAnimator}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamItemAnimatorTest {
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private ViewActionManager mViewActionManager;
    @Mock
    private FeedViewHolder mFeedViewHolder;

    private StreamItemAnimatorForTest mStreamItemAnimator;

    @Before
    public void setup() {
        initMocks(this);
        mStreamItemAnimator =
                new StreamItemAnimatorForTest(mContentChangedListener, mViewActionManager);
    }

    @Test
    public void testOnAnimationFinished() {
        mStreamItemAnimator.onAnimationFinished(mFeedViewHolder);
        verify(mContentChangedListener).onContentChanged();
        verify(mViewActionManager, never()).onAnimationFinished();
    }

    @Test
    public void testOnAnimationFinished_visible() {
        mStreamItemAnimator.setStreamVisibility(true);
        mStreamItemAnimator.onAnimationFinished(mFeedViewHolder);
        verify(mContentChangedListener).onContentChanged();
        verify(mViewActionManager).onAnimationFinished();
    }

    @Test
    public void setStreamVisibility_toVisible_endsAnimations() {
        mStreamItemAnimator.setStreamVisibility(true);
        assertThat(mStreamItemAnimator.mAnimationsEnded).isTrue();
    }

    private static class StreamItemAnimatorForTest extends StreamItemAnimator {
        private boolean mAnimationsEnded;

        StreamItemAnimatorForTest(
                ContentChangedListener contentChangedListener, ViewActionManager actionManager) {
            super(contentChangedListener, actionManager);
        }

        @Override
        public void endAnimations() {
            super.endAnimations();
            mAnimationsEnded = true;
        }
    }
}

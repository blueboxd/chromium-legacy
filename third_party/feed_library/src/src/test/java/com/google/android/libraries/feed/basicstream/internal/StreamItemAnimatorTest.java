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
  @Mock private ContentChangedListener contentChangedListener;
  @Mock private FeedViewHolder feedViewHolder;

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

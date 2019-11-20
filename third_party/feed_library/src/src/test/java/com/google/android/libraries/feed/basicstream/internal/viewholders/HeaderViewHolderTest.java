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

package com.google.android.libraries.feed.basicstream.internal.viewholders;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.widget.FrameLayout;
import com.google.android.libraries.feed.api.client.stream.Header;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link HeaderViewHolder}. */
@RunWith(RobolectricTestRunner.class)
public class HeaderViewHolderTest {

  private Context context;
  private HeaderViewHolder headerViewHolder;
  private FrameLayout frameLayout;
  private FrameLayout headerView;

  @Mock private Header header;
  @Mock private SwipeNotifier swipeNotifier;

  @Before
  public void setUp() {
    initMocks(this);
    context = Robolectric.buildActivity(Activity.class).get();
    frameLayout = new FrameLayout(context);
    headerView = new FrameLayout(context);
    headerViewHolder = new HeaderViewHolder(frameLayout);
    when(header.getView()).thenReturn(headerView);
  }

  @Test
  public void testBind() {
    headerViewHolder.bind(header, swipeNotifier);

    assertThat(frameLayout.getChildAt(0)).isSameInstanceAs(headerView);
  }

  @Test
  public void testBind_removesHeaderFromPreviousView_ifHeaderViewAlreadyHasParent() {
    FrameLayout parentView = new FrameLayout(context);
    parentView.addView(headerView);

    headerViewHolder.bind(header, swipeNotifier);

    assertThat(parentView.getChildCount()).isEqualTo(0);
    assertThat(frameLayout.getChildAt(0)).isSameInstanceAs(headerView);
  }

  @Test
  public void testUnbind() {
    headerViewHolder.bind(header, swipeNotifier);
  }

  @Test
  public void testCanSwipe() {
    when(header.isDismissible()).thenReturn(true);
    headerViewHolder.bind(header, swipeNotifier);

    assertThat(headerViewHolder.canSwipe()).isTrue();
  }

  @Test
  public void testCanSwipe_returnsFalse_whenViewHolderNotBound() {
    when(header.isDismissible()).thenReturn(true);

    assertThat(headerViewHolder.canSwipe()).isFalse();
  }

  @Test
  public void testOnSwiped() {
    headerViewHolder.bind(header, swipeNotifier);

    headerViewHolder.onSwiped();

    verify(swipeNotifier).onSwiped();
  }

  @Test
  public void testOnSwiped_doesNotCallOnSwiped_whenViewHolderNotBound() {
    headerViewHolder.onSwiped();

    verify(swipeNotifier, never()).onSwiped();
  }
}

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

package com.google.android.libraries.feed.feedapplifecyclelistener;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Test class for {@link FeedAppLifecycleListener} */
@RunWith(RobolectricTestRunner.class)
public class FeedAppLifecycleListenerTest {
  @Mock private FeedLifecycleListener lifecycleListener;
  @Mock private ThreadUtils threadUtils;
  private FeedAppLifecycleListener appLifecycleListener;

  @Before
  public void setup() {
    initMocks(this);
    appLifecycleListener = new FeedAppLifecycleListener(threadUtils);
    appLifecycleListener.registerObserver(lifecycleListener);
  }

  @Test
  public void onEnterForeground() {
    appLifecycleListener.onEnterForeground();
    verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.ENTER_FOREGROUND);
  }

  @Test
  public void onEnterBackground() {
    appLifecycleListener.onEnterBackground();
    verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
  }

  @Test
  public void onClearAll() {
    appLifecycleListener.onClearAll();
    verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.CLEAR_ALL);
  }

  @Test
  public void onClearAllWithRefresh() {
    appLifecycleListener.onClearAllWithRefresh();
    verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.CLEAR_ALL_WITH_REFRESH);
  }

  @Test
  public void onInitialize() {
    appLifecycleListener.initialize();
    verify(lifecycleListener).onLifecycleEvent(LifecycleEvent.INITIALIZE);
  }
}

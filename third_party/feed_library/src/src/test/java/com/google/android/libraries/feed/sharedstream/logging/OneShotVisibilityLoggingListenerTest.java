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

package com.google.android.libraries.feed.sharedstream.logging;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link OneShotVisibilityLoggingListener}. */
@RunWith(RobolectricTestRunner.class)
public class OneShotVisibilityLoggingListenerTest {

  @Mock private LoggingListener loggingListener;

  private OneShotVisibilityLoggingListener oneShotVisibilityLoggingListener;

  @Before
  public void setUp() {
    initMocks(this);
    oneShotVisibilityLoggingListener = new OneShotVisibilityLoggingListener(loggingListener);
  }

  @Test
  public void testOnViewVisible() {
    oneShotVisibilityLoggingListener.onViewVisible();

    verify(loggingListener).onViewVisible();
  }

  @Test
  public void testOnViewVisible_onlyNotifiesListenerOnce() {
    oneShotVisibilityLoggingListener.onViewVisible();
    reset(loggingListener);

    oneShotVisibilityLoggingListener.onViewVisible();

    verify(loggingListener, never()).onViewVisible();
  }

  @Test
  public void testOnContentClicked() {
    oneShotVisibilityLoggingListener.onContentClicked();

    verify(loggingListener).onContentClicked();
  }
}

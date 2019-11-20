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

package com.google.android.libraries.feed.sharedstream.scroll;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ScrollType;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ScrollLogger}. */
@RunWith(RobolectricTestRunner.class)
public final class ScrollLoggerTest {

  @Mock private BasicLoggingApi basicLoggingApi;
  private ScrollLogger scrollLogger;

  @Before
  public void setUp() {
    initMocks(this);
    scrollLogger = new ScrollLogger(basicLoggingApi);
  }

  @Test
  public void testLogsScroll() {
    int scrollAmount = 100;
    scrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    verify(basicLoggingApi).onScroll(ScrollType.STREAM_SCROLL, scrollAmount);
  }

  @Test
  public void testLogsNoScroll_tooSmall() {
    int scrollAmount = 1;
    scrollLogger.handleScroll(ScrollType.STREAM_SCROLL, scrollAmount);
    verify(basicLoggingApi, never()).onScroll(ScrollType.STREAM_SCROLL, scrollAmount);
  }
}

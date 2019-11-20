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

package com.google.android.libraries.feed.feedmodelprovider;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.search.now.feed.client.StreamDataProto.UiContext;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link FeedModelProviderFactory}. */
@RunWith(RobolectricTestRunner.class)
public class FeedModelProviderFactoryTest {

  private final FakeBasicLoggingApi fakeBasicLoggingApi = new FakeBasicLoggingApi();
  @Mock private ThreadUtils threadUtils;
  @Mock private TimingUtils timingUtils;
  @Mock private FeedSessionManager feedSessionManager;
  @Mock private Configuration config;
  @Mock private TaskQueue taskQueue;

  private MainThreadRunner mainThreadRunner;

  @Before
  public void setUp() {
    initMocks(this);

    mainThreadRunner = FakeMainThreadRunner.queueAllTasks();

    when(config.getValueOrDefault(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE, 0L)).thenReturn(0L);
    when(config.getValueOrDefault(ConfigKey.NON_CACHED_PAGE_SIZE, 0L)).thenReturn(0L);
    when(config.getValueOrDefault(ConfigKey.NON_CACHED_MIN_PAGE_SIZE, 0L)).thenReturn(0L);
  }

  @Test
  public void testModelProviderFactory() {
    FeedModelProviderFactory factory =
        new FeedModelProviderFactory(
            feedSessionManager,
            threadUtils,
            timingUtils,
            taskQueue,
            mainThreadRunner,
            config,
            fakeBasicLoggingApi);
    assertThat(factory.createNew(null, UiContext.getDefaultInstance())).isNotNull();
  }
}

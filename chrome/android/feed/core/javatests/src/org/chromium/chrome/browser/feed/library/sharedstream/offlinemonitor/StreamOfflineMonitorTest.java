// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link StreamOfflineMonitor}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StreamOfflineMonitorTest {
    private static final String URL = "google.com";

    @Mock
    private Consumer<Boolean> mOfflineStatusConsumer1;
    @Mock
    private Consumer<Boolean> mOfflineStatusConsumer2;
    @Mock
    private OfflineIndicatorApi mOfflineIndicatorApi;
    @Captor
    private ArgumentCaptor<List<String>> mListArgumentCaptor;

    private StreamOfflineMonitor mStreamOfflineMonitor;

    @Before
    public void setup() {
        initMocks(this);
        mStreamOfflineMonitor = new StreamOfflineMonitor(mOfflineIndicatorApi);
    }

    @Test
    public void testIsAvailableOffline() {
        assertThat(mStreamOfflineMonitor.isAvailableOffline(URL)).isFalse();
        mStreamOfflineMonitor.updateOfflineStatus(URL, true);
        assertThat(mStreamOfflineMonitor.isAvailableOffline(URL)).isTrue();
        mStreamOfflineMonitor.updateOfflineStatus(URL, false);
        assertThat(mStreamOfflineMonitor.isAvailableOffline(URL)).isFalse();
    }

    @Test
    public void testNotifyListeners() {
        mStreamOfflineMonitor.addOfflineStatusConsumer(URL, mOfflineStatusConsumer1);

        mStreamOfflineMonitor.updateOfflineStatus(URL, true);

        verify(mOfflineStatusConsumer1).accept(true);

        mStreamOfflineMonitor.updateOfflineStatus(URL, false);

        verify(mOfflineStatusConsumer1).accept(false);
    }

    @Test
    public void testNotify_multipleListeners() {
        mStreamOfflineMonitor.addOfflineStatusConsumer(URL, mOfflineStatusConsumer1);
        mStreamOfflineMonitor.addOfflineStatusConsumer(URL, mOfflineStatusConsumer2);
        mStreamOfflineMonitor.updateOfflineStatus(URL, true);

        verify(mOfflineStatusConsumer1).accept(true);
        verify(mOfflineStatusConsumer2).accept(true);
    }

    @Test
    public void testRemoveConsumer() {
        mStreamOfflineMonitor.addOfflineStatusConsumer(URL, mOfflineStatusConsumer1);

        mStreamOfflineMonitor.removeOfflineStatusConsumer(URL, mOfflineStatusConsumer1);

        mStreamOfflineMonitor.updateOfflineStatus(URL, true);

        verify(mOfflineStatusConsumer1, never()).accept(anyBoolean());
    }

    @Test
    public void testNotifyOnlyOnce() {
        mStreamOfflineMonitor.addOfflineStatusConsumer(URL, mOfflineStatusConsumer1);

        mStreamOfflineMonitor.updateOfflineStatus(URL, true);
        mStreamOfflineMonitor.updateOfflineStatus(URL, true);
        mStreamOfflineMonitor.updateOfflineStatus(URL, false);
        mStreamOfflineMonitor.updateOfflineStatus(URL, false);

        verify(mOfflineStatusConsumer1, times(1)).accept(true);
        verify(mOfflineStatusConsumer1, times(1)).accept(false);
    }

    @Test
    public void testRequestOfflineStatusForNewContent() {
        String url1 = "gmail.com";
        String url2 = "mail.google.com";

        // Checking if they are offline adds them to the list of articles the StreamOfflineMonitor
        // will ask about.
        mStreamOfflineMonitor.isAvailableOffline(URL);
        mStreamOfflineMonitor.isAvailableOffline(url1);
        mStreamOfflineMonitor.isAvailableOffline(url2);

        mStreamOfflineMonitor.requestOfflineStatusForNewContent();
        verify(mOfflineIndicatorApi)
                .getOfflineStatus(mListArgumentCaptor.capture(),
                        ArgumentMatchers.<Consumer<List<String>>>any());

        assertThat(mListArgumentCaptor.getValue())
                .containsExactlyElementsIn(Arrays.asList(URL, url1, url2));
    }

    @Test
    public void testRequestOfflineStatusForNewContent_onlyRequestsOnce() {
        // Checking if it is offline adds it to the list of articles the StreamOfflineMonitor will
        // ask about.
        mStreamOfflineMonitor.isAvailableOffline(URL);

        mStreamOfflineMonitor.requestOfflineStatusForNewContent();
        reset(mOfflineIndicatorApi);

        mStreamOfflineMonitor.requestOfflineStatusForNewContent();

        verify(mOfflineIndicatorApi, never())
                .getOfflineStatus(eq(Collections.emptyList()),
                        ArgumentMatchers.<Consumer<List<String>>>any());
    }

    @Test
    public void testRequestOfflineStatusForNewContent_noUrls() {
        mStreamOfflineMonitor.requestOfflineStatusForNewContent();

        verify(mOfflineIndicatorApi, never())
                .getOfflineStatus(eq(Collections.emptyList()),
                        ArgumentMatchers.<Consumer<List<String>>>any());
    }

    @Test
    public void testOnDestroy_clearsConsumersMap() {
        mStreamOfflineMonitor.addOfflineStatusConsumer(URL, mOfflineStatusConsumer1);

        // Should clear out all listeners
        mStreamOfflineMonitor.onDestroy();

        mStreamOfflineMonitor.updateOfflineStatus(URL, true);

        verifyZeroInteractions(mOfflineStatusConsumer1);
    }

    @Test
    public void testOnDestroy_detachesFromApi() {
        mStreamOfflineMonitor.onDestroy();

        verify(mOfflineIndicatorApi).removeOfflineStatusListener(mStreamOfflineMonitor);
    }
}

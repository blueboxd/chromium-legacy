// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.offlinemonitor;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.common.functional.Consumer;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link StreamOfflineMonitor}. */
@RunWith(RobolectricTestRunner.class)
public class StreamOfflineMonitorTest {
    private static final String URL = "google.com";

    @Mock
    private Consumer<Boolean> offlineStatusConsumer1;
    @Mock
    private Consumer<Boolean> offlineStatusConsumer2;
    @Mock
    private OfflineIndicatorApi offlineIndicatorApi;
    @Captor
    private ArgumentCaptor<List<String>> listArgumentCaptor;

    private StreamOfflineMonitor streamOfflineMonitor;

    @Before
    public void setup() {
        initMocks(this);
        streamOfflineMonitor = new StreamOfflineMonitor(offlineIndicatorApi);
    }

    @Test
    public void testIsAvailableOffline() {
        assertThat(streamOfflineMonitor.isAvailableOffline(URL)).isFalse();
        streamOfflineMonitor.updateOfflineStatus(URL, true);
        assertThat(streamOfflineMonitor.isAvailableOffline(URL)).isTrue();
        streamOfflineMonitor.updateOfflineStatus(URL, false);
        assertThat(streamOfflineMonitor.isAvailableOffline(URL)).isFalse();
    }

    @Test
    public void testNotifyListeners() {
        streamOfflineMonitor.addOfflineStatusConsumer(URL, offlineStatusConsumer1);

        streamOfflineMonitor.updateOfflineStatus(URL, true);

        verify(offlineStatusConsumer1).accept(true);

        streamOfflineMonitor.updateOfflineStatus(URL, false);

        verify(offlineStatusConsumer1).accept(false);
    }

    @Test
    public void testNotify_multipleListeners() {
        streamOfflineMonitor.addOfflineStatusConsumer(URL, offlineStatusConsumer1);
        streamOfflineMonitor.addOfflineStatusConsumer(URL, offlineStatusConsumer2);
        streamOfflineMonitor.updateOfflineStatus(URL, true);

        verify(offlineStatusConsumer1).accept(true);
        verify(offlineStatusConsumer2).accept(true);
    }

    @Test
    public void testRemoveConsumer() {
        streamOfflineMonitor.addOfflineStatusConsumer(URL, offlineStatusConsumer1);

        streamOfflineMonitor.removeOfflineStatusConsumer(URL, offlineStatusConsumer1);

        streamOfflineMonitor.updateOfflineStatus(URL, true);

        verify(offlineStatusConsumer1, never()).accept(anyBoolean());
    }

    @Test
    public void testNotifyOnlyOnce() {
        streamOfflineMonitor.addOfflineStatusConsumer(URL, offlineStatusConsumer1);

        streamOfflineMonitor.updateOfflineStatus(URL, true);
        streamOfflineMonitor.updateOfflineStatus(URL, true);
        streamOfflineMonitor.updateOfflineStatus(URL, false);
        streamOfflineMonitor.updateOfflineStatus(URL, false);

        verify(offlineStatusConsumer1, times(1)).accept(true);
        verify(offlineStatusConsumer1, times(1)).accept(false);
    }

    @Test
    public void testRequestOfflineStatusForNewContent() {
        String url1 = "gmail.com";
        String url2 = "mail.google.com";

        // Checking if they are offline adds them to the list of articles the StreamOfflineMonitor
        // will ask about.
        streamOfflineMonitor.isAvailableOffline(URL);
        streamOfflineMonitor.isAvailableOffline(url1);
        streamOfflineMonitor.isAvailableOffline(url2);

        streamOfflineMonitor.requestOfflineStatusForNewContent();
        verify(offlineIndicatorApi)
                .getOfflineStatus(listArgumentCaptor.capture(),
                        ArgumentMatchers.<Consumer<List<String>>>any());

        assertThat(listArgumentCaptor.getValue())
                .containsExactlyElementsIn(Arrays.asList(URL, url1, url2));
    }

    @Test
    public void testRequestOfflineStatusForNewContent_onlyRequestsOnce() {
        // Checking if it is offline adds it to the list of articles the StreamOfflineMonitor will
        // ask about.
        streamOfflineMonitor.isAvailableOffline(URL);

        streamOfflineMonitor.requestOfflineStatusForNewContent();
        reset(offlineIndicatorApi);

        streamOfflineMonitor.requestOfflineStatusForNewContent();

        verify(offlineIndicatorApi, never())
                .getOfflineStatus(eq(Collections.emptyList()),
                        ArgumentMatchers.<Consumer<List<String>>>any());
    }

    @Test
    public void testRequestOfflineStatusForNewContent_noUrls() {
        streamOfflineMonitor.requestOfflineStatusForNewContent();

        verify(offlineIndicatorApi, never())
                .getOfflineStatus(eq(Collections.emptyList()),
                        ArgumentMatchers.<Consumer<List<String>>>any());
    }

    @Test
    public void testOnDestroy_clearsConsumersMap() {
        streamOfflineMonitor.addOfflineStatusConsumer(URL, offlineStatusConsumer1);

        // Should clear out all listeners
        streamOfflineMonitor.onDestroy();

        streamOfflineMonitor.updateOfflineStatus(URL, true);

        verifyZeroInteractions(offlineStatusConsumer1);
    }

    @Test
    public void testOnDestroy_detachesFromApi() {
        streamOfflineMonitor.onDestroy();

        verify(offlineIndicatorApi).removeOfflineStatusListener(streamOfflineMonitor);
    }
}

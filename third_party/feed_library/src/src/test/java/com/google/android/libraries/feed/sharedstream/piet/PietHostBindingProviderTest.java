// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.sharedstream.offlinemonitor.StreamOfflineMonitor;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.HostBindingData;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;
import com.google.search.now.ui.stream.StreamOfflineExtensionProto.OfflineExtension;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PietHostBindingProvider}. */
@RunWith(RobolectricTestRunner.class)
public class PietHostBindingProviderTest {
    private static final String URL = "url";
    private static final BindingValue OFFLINE_BINDING_VISIBILITY =
            BindingValue.newBuilder().setBindingId("offline").build();
    private static final BindingValue NOT_OFFLINE_BINDING_VISIBILITY =
            BindingValue.newBuilder().setBindingId("not-offline").build();
    public static final BindingValue OFFLINE_INDICATOR_BINDING =
            BindingValue.newBuilder()
                    .setHostBindingData(HostBindingData.newBuilder().setExtension(
                            OfflineExtension.offlineExtension,
                            OfflineExtension.newBuilder()
                                    .setUrl(URL)
                                    .setOfflineBinding(OFFLINE_BINDING_VISIBILITY)
                                    .setNotOfflineBinding(NOT_OFFLINE_BINDING_VISIBILITY)
                                    .build()))
                    .build();

    @Mock
    private HostBindingProvider hostHostBindingProvider;
    @Mock
    private StreamOfflineMonitor offlineMonitor;

    private static final ParameterizedText TEXT_PAYLOAD =
            ParameterizedText.newBuilder().setText("foo").build();

    private static final BindingValue BINDING_WITH_HOST_DATA =
            BindingValue.newBuilder()
                    .setHostBindingData(HostBindingData.newBuilder())
                    .setParameterizedText(TEXT_PAYLOAD)
                    .build();
    private static final BindingValue BINDING_WITHOUT_HOST_DATA =
            BindingValue.newBuilder().setParameterizedText(TEXT_PAYLOAD).build();

    private PietHostBindingProvider hostBindingProvider;
    private PietHostBindingProvider delegatingHostBindingProvider;

    @Before
    public void setUp() {
        initMocks(this);

        hostBindingProvider =
                new PietHostBindingProvider(/* hostBindingProvider */ null, offlineMonitor);
        delegatingHostBindingProvider =
                new PietHostBindingProvider(hostHostBindingProvider, offlineMonitor);
    }

    @Test
    public void testGetCustomElementDataBindingForValue() {
        assertThat(hostBindingProvider.getCustomElementDataBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetCustomElementDataBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("custom-element").build();
        when(hostHostBindingProvider.getCustomElementDataBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getCustomElementDataBindingForValue(
                           BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetParameterizedTextBindingForValue() {
        assertThat(hostBindingProvider.getParameterizedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetParameterizedTextBindingForValue_delegating() {
        BindingValue hostBinding =
                BindingValue.newBuilder().setBindingId("parameterized-text").build();
        when(hostHostBindingProvider.getParameterizedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getParameterizedTextBindingForValue(
                           BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetChunkedTextBindingForValue() {
        assertThat(hostBindingProvider.getChunkedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetChunkedTextBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("chunked-text").build();
        when(hostHostBindingProvider.getChunkedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(
                delegatingHostBindingProvider.getChunkedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetImageBindingForValue() {
        assertThat(hostBindingProvider.getChunkedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetImageBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("image").build();
        when(hostHostBindingProvider.getImageBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getImageBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetActionsBindingForValue() {
        assertThat(hostBindingProvider.getActionsBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetActionsBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("actions").build();
        when(hostHostBindingProvider.getActionsBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getActionsBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetGridCellWidthBindingForValue() {
        assertThat(hostBindingProvider.getGridCellWidthBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetGridCellWidthBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("gridcell").build();
        when(hostHostBindingProvider.getGridCellWidthBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getGridCellWidthBindingForValue(
                           BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetLogDataBindingForValue() {
        assertThat(hostBindingProvider.getLogDataBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetLogDataBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(hostHostBindingProvider.getLogDataBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getLogDataBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetTemplateBindingForValue() {
        assertThat(hostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetTemplateBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(hostHostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetStyleBindingForValue() {
        assertThat(hostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetStyleBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(hostHostBindingProvider.getStyleBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(delegatingHostBindingProvider.getStyleBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetVisibilityBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(hostHostBindingProvider.getVisibilityBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(
                delegatingHostBindingProvider.getVisibilityBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetVisibilityBindingForValue_noOfflineExtension() {
        assertThat(hostBindingProvider.getVisibilityBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetVisibilityBindingForValue_offlineExtension_notOffline() {
        when(offlineMonitor.isAvailableOffline(URL)).thenReturn(false);
        assertThat(hostBindingProvider.getVisibilityBindingForValue(OFFLINE_INDICATOR_BINDING))
                .isEqualTo(NOT_OFFLINE_BINDING_VISIBILITY);
    }

    @Test
    public void testGetVisibilityBindingForValue_offlineExtension_offline() {
        when(offlineMonitor.isAvailableOffline(URL)).thenReturn(true);
        assertThat(hostBindingProvider.getVisibilityBindingForValue(OFFLINE_INDICATOR_BINDING))
                .isEqualTo(OFFLINE_BINDING_VISIBILITY);
    }
}

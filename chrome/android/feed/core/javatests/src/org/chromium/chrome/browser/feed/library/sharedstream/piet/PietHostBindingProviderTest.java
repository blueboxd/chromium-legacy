// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.HostBindingData;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.components.feed.core.proto.ui.stream.StreamOfflineExtensionProto.OfflineExtension;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link PietHostBindingProvider}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    private HostBindingProvider mHostHostBindingProvider;
    @Mock
    private StreamOfflineMonitor mOfflineMonitor;

    private static final ParameterizedText TEXT_PAYLOAD =
            ParameterizedText.newBuilder().setText("foo").build();

    private static final BindingValue BINDING_WITH_HOST_DATA =
            BindingValue.newBuilder()
                    .setHostBindingData(HostBindingData.newBuilder())
                    .setParameterizedText(TEXT_PAYLOAD)
                    .build();
    private static final BindingValue BINDING_WITHOUT_HOST_DATA =
            BindingValue.newBuilder().setParameterizedText(TEXT_PAYLOAD).build();

    private PietHostBindingProvider mHostBindingProvider;
    private PietHostBindingProvider mDelegatingHostBindingProvider;

    @Before
    public void setUp() {
        initMocks(this);

        mHostBindingProvider =
                new PietHostBindingProvider(/* hostBindingProvider */ null, mOfflineMonitor);
        mDelegatingHostBindingProvider =
                new PietHostBindingProvider(mHostHostBindingProvider, mOfflineMonitor);
    }

    @Test
    public void testGetCustomElementDataBindingForValue() {
        assertThat(mHostBindingProvider.getCustomElementDataBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetCustomElementDataBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("custom-element").build();
        when(mHostHostBindingProvider.getCustomElementDataBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getCustomElementDataBindingForValue(
                           BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetParameterizedTextBindingForValue() {
        assertThat(mHostBindingProvider.getParameterizedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetParameterizedTextBindingForValue_delegating() {
        BindingValue hostBinding =
                BindingValue.newBuilder().setBindingId("parameterized-text").build();
        when(mHostHostBindingProvider.getParameterizedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getParameterizedTextBindingForValue(
                           BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetChunkedTextBindingForValue() {
        assertThat(mHostBindingProvider.getChunkedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetChunkedTextBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("chunked-text").build();
        when(mHostHostBindingProvider.getChunkedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getChunkedTextBindingForValue(
                           BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetImageBindingForValue() {
        assertThat(mHostBindingProvider.getChunkedTextBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetImageBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("image").build();
        when(mHostHostBindingProvider.getImageBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getImageBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetActionsBindingForValue() {
        assertThat(mHostBindingProvider.getActionsBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetActionsBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("actions").build();
        when(mHostHostBindingProvider.getActionsBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getActionsBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetGridCellWidthBindingForValue() {
        assertThat(mHostBindingProvider.getGridCellWidthBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetGridCellWidthBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("gridcell").build();
        when(mHostHostBindingProvider.getGridCellWidthBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getGridCellWidthBindingForValue(
                           BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetLogDataBindingForValue() {
        assertThat(mHostBindingProvider.getLogDataBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetLogDataBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(mHostHostBindingProvider.getLogDataBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getLogDataBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetTemplateBindingForValue() {
        assertThat(mHostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetTemplateBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(mHostHostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(
                mDelegatingHostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetStyleBindingForValue() {
        assertThat(mHostBindingProvider.getTemplateBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetStyleBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(mHostHostBindingProvider.getStyleBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(mDelegatingHostBindingProvider.getStyleBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetVisibilityBindingForValue_delegating() {
        BindingValue hostBinding = BindingValue.newBuilder().setBindingId("ved").build();
        when(mHostHostBindingProvider.getVisibilityBindingForValue(BINDING_WITH_HOST_DATA))
                .thenReturn(hostBinding);

        assertThat(
                mDelegatingHostBindingProvider.getVisibilityBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(hostBinding);
    }

    @Test
    public void testGetVisibilityBindingForValue_noOfflineExtension() {
        assertThat(mHostBindingProvider.getVisibilityBindingForValue(BINDING_WITH_HOST_DATA))
                .isEqualTo(BINDING_WITHOUT_HOST_DATA);
    }

    @Test
    public void testGetVisibilityBindingForValue_offlineExtension_notOffline() {
        when(mOfflineMonitor.isAvailableOffline(URL)).thenReturn(false);
        assertThat(mHostBindingProvider.getVisibilityBindingForValue(OFFLINE_INDICATOR_BINDING))
                .isEqualTo(NOT_OFFLINE_BINDING_VISIBILITY);
    }

    @Test
    public void testGetVisibilityBindingForValue_offlineExtension_offline() {
        when(mOfflineMonitor.isAvailableOffline(URL)).thenReturn(true);
        assertThat(mHostBindingProvider.getVisibilityBindingForValue(OFFLINE_INDICATOR_BINDING))
                .isEqualTo(OFFLINE_BINDING_VISIBILITY);
    }
}

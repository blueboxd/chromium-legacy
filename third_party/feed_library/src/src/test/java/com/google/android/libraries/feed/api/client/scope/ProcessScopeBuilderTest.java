// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.client.scope;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.withSettings;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;

import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.host.proto.ProtoExtensionProvider;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.host.storage.JournalStorage;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.common.util.concurrent.MoreExecutors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;

/** Tests for {@link ProcessScopeBuilder}. */
@RunWith(RobolectricTestRunner.class)
public class ProcessScopeBuilderTest {
    // Mocks for required fields
    @Mock
    private BasicLoggingApi basicLoggingApi;
    @Mock
    private NetworkClient networkClient;
    @Mock
    private SchedulerApi schedulerApi;
    @Mock
    private ApplicationInfo applicationInfo;
    @Mock
    private TooltipSupportedApi tooltipSupportedApi;

    // Mocks for optional fields
    @Mock
    private ThreadUtils threadUtils;

    private final ProtoExtensionProvider protoExtensionProvider = ArrayList::new;
    private Configuration configuration = new Configuration.Builder().build();
    private Context context;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
    }

    @Test
    public void testBasicBuild() {
        // No crash should happen.
        ProcessScope processScope =
                new ProcessScopeBuilder(configuration, MoreExecutors.newDirectExecutorService(),
                        basicLoggingApi, networkClient, schedulerApi, DebugBehavior.VERBOSE,
                        context, applicationInfo, tooltipSupportedApi)
                        .setJournalStorageDirect(mock(JournalStorageDirect.class))
                        .setContentStorageDirect(mock(ContentStorageDirect.class))
                        .build();

        assertThat(processScope.getRequestManager()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
        assertThat(processScope.getKnownContent()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
    }

    @Test
    public void testComplexBuild() {
        // No crash should happen.
        ProcessScope processScope =
                new ProcessScopeBuilder(configuration, MoreExecutors.newDirectExecutorService(),
                        basicLoggingApi, networkClient, schedulerApi, DebugBehavior.VERBOSE,
                        context, applicationInfo, tooltipSupportedApi)
                        .setProtoExtensionProvider(protoExtensionProvider)
                        .setJournalStorageDirect(mock(JournalStorageDirect.class))
                        .setContentStorageDirect(mock(ContentStorageDirect.class))
                        .build();

        assertThat(processScope.getRequestManager()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
    }

    @Test
    public void testDirectStorage() {
        ContentStorageDirect contentStorageDirect = mock(ContentStorageDirect.class);
        JournalStorageDirect journalStorageDirect = mock(JournalStorageDirect.class);
        ProcessScopeBuilder builder = new ProcessScopeBuilder(configuration,
                MoreExecutors.newDirectExecutorService(), basicLoggingApi, networkClient,
                schedulerApi, DebugBehavior.VERBOSE, context, applicationInfo, tooltipSupportedApi)
                                              .setContentStorageDirect(contentStorageDirect)
                                              .setJournalStorageDirect(journalStorageDirect);
        assertThat(builder.contentStorage).isEqualTo(contentStorageDirect);
        assertThat(builder.journalStorage).isEqualTo(journalStorageDirect);
    }

    @Test
    public void testStorage_direct() {
        configuration = new Configuration.Builder().put(ConfigKey.USE_DIRECT_STORAGE, true).build();
        ContentStorage contentStorageDirect = mock(
                ContentStorage.class, withSettings().extraInterfaces(ContentStorageDirect.class));
        JournalStorage journalStorageDirect = mock(
                JournalStorage.class, withSettings().extraInterfaces(JournalStorageDirect.class));
        ProcessScopeBuilder builder = new ProcessScopeBuilder(configuration,
                MoreExecutors.newDirectExecutorService(), basicLoggingApi, networkClient,
                schedulerApi, DebugBehavior.VERBOSE, context, applicationInfo, tooltipSupportedApi)
                                              .setContentStorage(contentStorageDirect)
                                              .setJournalStorage(journalStorageDirect);
        MainThreadRunner mainThreadRunner = new MainThreadRunner();
        assertThat(builder.buildContentStorage(mainThreadRunner)).isEqualTo(contentStorageDirect);
        assertThat(builder.buildJournalStorage(mainThreadRunner)).isEqualTo(journalStorageDirect);
    }

    @Test
    public void testStorage_wrapped() {
        configuration =
                new Configuration.Builder().put(ConfigKey.USE_DIRECT_STORAGE, false).build();
        ContentStorage contentStorageDirect = mock(
                ContentStorage.class, withSettings().extraInterfaces(ContentStorageDirect.class));
        JournalStorage journalStorageDirect = mock(
                JournalStorage.class, withSettings().extraInterfaces(JournalStorageDirect.class));
        ProcessScopeBuilder builder = new ProcessScopeBuilder(configuration,
                MoreExecutors.newDirectExecutorService(), basicLoggingApi, networkClient,
                schedulerApi, DebugBehavior.VERBOSE, context, applicationInfo, tooltipSupportedApi)
                                              .setContentStorage(contentStorageDirect)
                                              .setJournalStorage(journalStorageDirect);
        assertThat(builder.contentStorage).isNotEqualTo(contentStorageDirect);
        assertThat(builder.journalStorage).isNotEqualTo(journalStorageDirect);
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import static android.app.NotificationManager.IMPORTANCE_NONE;
import static android.app.NotificationManager.IMPORTANCE_DEFAULT;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.TWA_DISCLOSURE_INITIAL;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.TWA_DISCLOSURE_SUBSEQUENT;

import android.app.NotificationChannel;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureInfobar;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;

import dagger.Lazy;

/**
 * Tests for {@link DisclosureUiPicker}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O)
@Features.EnableFeatures({ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE})
public class DisclosureUiPickerTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock public DisclosureInfobar mInfobar;
    @Mock public DisclosureSnackbar mSnackbar;
    @Mock public DisclosureNotification mNotification;

    @Mock public NotificationManagerProxy mNotificationManager;
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;

    private DisclosureUiPicker mPicker;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPicker = new DisclosureUiPicker(
                new FilledLazy<>(mInfobar),
                new FilledLazy<>(mSnackbar),
                new FilledLazy<>(mNotification),
                mNotificationManager,
                mLifecycleDispatcher);
    }

    @Test
    @Feature("TrustedWebActivities")
    @Features.DisableFeatures({ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE})
    public void picksInfobar_whenFeatureDisabled() {
        mPicker.onFinishNativeInitialization();
        verify(mInfobar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksSnackbar_whenAllNotificationsDisabled() {
        setNotificationsEnabled(false);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksSnackbar_whenInitialChannelIsDisabled() {
        setNotificationsEnabled(true);
        setChannelEnabled(TWA_DISCLOSURE_INITIAL, false);
        setChannelEnabled(TWA_DISCLOSURE_SUBSEQUENT, true);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksSnackbar_whenSubsequentChannelIsDisabled() {
        setNotificationsEnabled(true);
        setChannelEnabled(TWA_DISCLOSURE_INITIAL, true);
        setChannelEnabled(TWA_DISCLOSURE_SUBSEQUENT, false);

        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
    }

    @Test
    @Feature("TrustedWebActivities")
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void doesntCheckChannelsOnPreO() {
        setNotificationsEnabled(true);

        mPicker.onFinishNativeInitialization();
        verify(mNotification).onStartWithNative();

        verify(mNotificationManager, never()).getNotificationChannel(any());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void picksNotification() {
        setNotificationsEnabled(true);
        setChannelEnabled(TWA_DISCLOSURE_INITIAL, true);
        setChannelEnabled(TWA_DISCLOSURE_SUBSEQUENT, true);

        mPicker.onFinishNativeInitialization();
        verify(mNotification).onStartWithNative();
    }

    private void setNotificationsEnabled(boolean enabled) {
        when(mNotificationManager.areNotificationsEnabled()).thenReturn(enabled);
    }

    private void setChannelEnabled(String channelId, boolean enabled) {
        NotificationChannel channel = Mockito.mock(NotificationChannel.class);
        when(channel.getImportance()).thenReturn(enabled ? IMPORTANCE_DEFAULT : IMPORTANCE_NONE);
        when(mNotificationManager.getNotificationChannel(eq(channelId))).thenReturn(channel);
    }

    private static class FilledLazy<T> implements Lazy<T> {
        private final T mContents;

        FilledLazy(T contents) {
            mContents = contents;
        }

        @Override
        public T get() {
            return mContents;
        }
    }
}

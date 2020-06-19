// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.media.AudioManager;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.media.ui.ChromeMediaNotificationControllerDelegate.ListenerService;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationListener;
import org.chromium.media_session.mojom.MediaSessionAction;

/**
 * JUnit tests for checking {@link ChromeMediaNotificationControllerDelegate.ListenerService}
 * handles intent actions correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationServiceActionsTest extends MediaNotificationTestBase {
    @Test
    public void testProcessIntentWithNoAction() {
        setUpServiceAndClearInvocations();
        MediaNotificationController controller = getController();
        doNothing().when(controller).onServiceStarted(any(ListenerService.class));
        assertTrue(mService.processIntent(new Intent()));
        verify(controller).onServiceStarted(mService);
    }

    @Test
    public void testProcessIntentWithAction() {
        setUpService();
        MediaNotificationController controller = getController();
        Intent intentWithAction = new Intent().setAction("foo");
        assertTrue(mService.processIntent(intentWithAction));
        verify(controller).processAction(intentWithAction.getAction());
    }

    @Test
    public void testProcessNotificationButtonAction_Stop() {
        setUpService();

        MediaNotificationController controller = getController();
        ListenerService service = mService;

        mService.processIntent(new Intent(MediaNotificationController.ACTION_STOP));
        verify(controller).onStop(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
        verify(controller).stopListenerService();
    }

    @Test
    public void testProcessNotificationButtonAction_Swipe() {
        setUpService();

        MediaNotificationController controller = getController();
        ListenerService service = mService;

        mService.processIntent(new Intent(MediaNotificationController.ACTION_SWIPE));
        verify(controller).onStop(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
        verify(controller).stopListenerService();
    }

    @Test
    public void testProcessNotificationButtonAction_Cancel() {
        setUpService();

        MediaNotificationController controller = getController();
        ListenerService service = mService;

        mService.processIntent(new Intent(MediaNotificationController.ACTION_CANCEL));
        verify(controller).onStop(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
        verify(controller).stopListenerService();
    }

    @Test
    public void testProcessNotificationButtonAction_Play() {
        setUpService();

        mService.processIntent(new Intent(MediaNotificationController.ACTION_PLAY));
        verify(getController()).onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
    }

    @Test
    public void testProcessNotificationButtonAction_Pause() {
        setUpService();

        mService.processIntent(new Intent(MediaNotificationController.ACTION_PAUSE));
        verify(getController()).onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
    }

    @Test
    public void testProcessNotificationButtonAction_Noisy() {
        setUpService();

        mService.processIntent(new Intent(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
        verify(getController()).onPause(MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG);
    }

    @Test
    public void testProcessNotificationButtonAction_PreviousTrack() {
        setUpService();

        mService.processIntent(new Intent(MediaNotificationController.ACTION_PREVIOUS_TRACK));
        verify(getController()).onMediaSessionAction(MediaSessionAction.PREVIOUS_TRACK);
    }

    @Test
    public void testProcessNotificationButtonAction_NextTrack() {
        setUpService();

        mService.processIntent(new Intent(MediaNotificationController.ACTION_NEXT_TRACK));
        verify(getController()).onMediaSessionAction(MediaSessionAction.NEXT_TRACK);
    }

    @Test
    public void testProcessNotificationButtonAction_SeekForward() {
        setUpService();

        mService.processIntent(new Intent(MediaNotificationController.ACTION_SEEK_FORWARD));
        verify(getController()).onMediaSessionAction(MediaSessionAction.SEEK_FORWARD);
    }

    @Test
    public void testProcessNotificationButtonAction_SeekBackward() {
        setUpService();

        mService.processIntent(new Intent(MediaNotificationController.ACTION_SEEK_BACKWARD));
        verify(getController()).onMediaSessionAction(MediaSessionAction.SEEK_BACKWARD);
    }
}

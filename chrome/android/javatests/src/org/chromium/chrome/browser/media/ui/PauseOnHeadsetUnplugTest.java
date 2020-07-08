// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.content.Intent;
import android.media.AudioManager;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for checking whether the media are paused when unplugging the headset
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PauseOnHeadsetUnplugTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PATH =
            "/content/test/data/media/session/media-session.html";
    private static final String VIDEO_ID = "long-video";

    private EmbeddedTestServer mTestServer;

    @Test
    @SmallTest
    @FlakyTest
    public void testPause() throws IllegalArgumentException, TimeoutException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(tab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);
        waitForNotificationReady();

        simulateHeadsetUnplug();
        DOMUtils.waitForMediaPauseBeforeEnd(tab.getWebContents(), VIDEO_ID);
    }

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PATH));
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void waitForNotificationReady() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            return MediaNotificationManager.hasControllerForTesting(
                    R.id.media_playback_notification);
        });
    }

    private void simulateHeadsetUnplug() {
        Intent i = new Intent(InstrumentationRegistry.getTargetContext(),
                ChromeMediaNotificationControllerDelegate.PlaybackListenerService.class);
        i.setAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY);

        InstrumentationRegistry.getContext().startService(i);
    }
}

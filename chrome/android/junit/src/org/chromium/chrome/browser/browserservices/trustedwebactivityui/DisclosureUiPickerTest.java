// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
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

import dagger.Lazy;

/**
 * Tests for {@link DisclosureUiPicker}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisclosureUiPickerTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock public DisclosureInfobar mInfobar;
    @Mock public DisclosureSnackbar mSnackbar;
    @Mock public DisclosureNotification mNotification;

    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;

    private DisclosureUiPicker mPicker;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPicker = new DisclosureUiPicker(
                new FilledLazy<>(mInfobar),
                new FilledLazy<>(mSnackbar),
                new FilledLazy<>(mNotification),
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
    @Features.EnableFeatures({ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE})
    public void picksSnackbar_whenFeatureEnabled() {
        mPicker.onFinishNativeInitialization();
        verify(mSnackbar).showIfNeeded();
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

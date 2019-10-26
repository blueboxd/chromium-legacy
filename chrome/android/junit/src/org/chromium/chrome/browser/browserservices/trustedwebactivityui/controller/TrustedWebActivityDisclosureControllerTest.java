// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier.VerificationState;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier.VerificationStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

/**
 * Tests for {@link TrustedWebActivityDisclosureController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityDisclosureControllerTest {
    private static final String CLIENT_PACKAGE = "com.example.twaclient";
    private static final Origin ORIGIN = new Origin("com.example.twa");

    @Mock public ChromePreferenceManager mPreferences;
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock public Verifier mVerifier;
    @Mock public TrustedWebActivityUmaRecorder mRecorder;

    @Captor public ArgumentCaptor<Runnable> mVerificationObserverCaptor;

    public TrustedWebActivityModel mModel = new TrustedWebActivityModel();

    private TrustedWebActivityDisclosureController mDisclosureController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(CLIENT_PACKAGE).when(mVerifier).getClientPackageName();
        doNothing().when(mVerifier).addVerificationObserver(mVerificationObserverCaptor.capture());
        doReturn(false).when(mPreferences).hasUserAcceptedTwaDisclosureForPackage(anyString());

        mDisclosureController = new TrustedWebActivityDisclosureController(
                mPreferences, mModel, mLifecycleDispatcher, mVerifier, mRecorder);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showsWhenOriginVerified() {
        enterVerifiedOrigin();
        assertSnackbarShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void dismissesWhenLeavingVerifiedOrigin() {
        enterVerifiedOrigin();
        exitVerifiedOrigin();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void showsAgainWhenReenteringTrustedOrigin() {
        enterVerifiedOrigin();
        exitVerifiedOrigin();
        enterVerifiedOrigin();
        assertSnackbarShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void noShowIfAlreadyAccepted() {
        doReturn(true).when(mPreferences).hasUserAcceptedTwaDisclosureForPackage(anyString());
        enterVerifiedOrigin();
        assertSnackbarNotShown();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void recordDismiss() {
        enterVerifiedOrigin();
        dismissSnackbar();
        verify(mPreferences).setUserAcceptedTwaDisclosureForPackage(CLIENT_PACKAGE);
    }

    private void enterVerifiedOrigin() {
        setVerificationState(new VerificationState(ORIGIN, VerificationStatus.SUCCESS));
    }

    private void exitVerifiedOrigin() {
        setVerificationState(new VerificationState(ORIGIN, VerificationStatus.FAILURE));
    }

    private void setVerificationState(VerificationState state) {
        doReturn(state).when(mVerifier).getState();
        for (Runnable observer : mVerificationObserverCaptor.getAllValues()) {
            observer.run();
        }
    }

    private void assertSnackbarShown() {
        assertEquals(DISCLOSURE_STATE_SHOWN, mModel.get(DISCLOSURE_STATE));
    }

    private void assertSnackbarNotShown() {
        assertEquals(DISCLOSURE_STATE_NOT_SHOWN, mModel.get(DISCLOSURE_STATE));
    }

    private void dismissSnackbar() {
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureAccepted();
    }
}

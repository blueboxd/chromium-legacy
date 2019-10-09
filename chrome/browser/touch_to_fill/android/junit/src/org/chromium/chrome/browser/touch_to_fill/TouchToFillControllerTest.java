// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CREDENTIAL_LIST;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VIEW_EVENT_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collections;

/**
 * Controller tests verify that the Touch To Fill controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TouchToFillControllerTest {
    private static final String TEST_URL = "www.example.xyz";
    private static final String TEST_MOBILE_URL = "www.example.xyz";
    private static final Credential ANA = new Credential("Ana", "S3cr3t", "Ana", null, false);
    private static final Credential BOB =
            new Credential("Bob", "*****", "Bob", TEST_MOBILE_URL, true);
    private static final Credential CARL = new Credential("Carl", "G3h3!m", "Carl", "", false);

    @Mock
    private TouchToFillComponent.Delegate mMockDelegate;

    private final TouchToFillMediator mMediator = new TouchToFillMediator();
    private final PropertyModel mModel = TouchToFillProperties.createDefaultModel(mMediator);

    public TouchToFillControllerTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() {
        mMediator.initialize(mMockDelegate, mModel);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mModel.get(CREDENTIAL_LIST));
        assertNotNull(mModel.get(VIEW_EVENT_LISTENER));
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(mModel.get(FORMATTED_URL), is(nullValue()));
    }

    @Test
    public void testShowCredentialsSetsFormattedUrl() {
        mMediator.showCredentials(TEST_URL, Arrays.asList(ANA, CARL, BOB));
        assertThat(mModel.get(FORMATTED_URL), is(TEST_URL));
    }

    @Test
    public void testShowCredentialsSetsCredentialList() {
        mMediator.showCredentials(TEST_URL, Arrays.asList(ANA, CARL, BOB));
        assertThat(mModel.get(CREDENTIAL_LIST).size(), is(3));
        assertThat(mModel.get(CREDENTIAL_LIST).get(0), is(ANA));
        assertThat(mModel.get(CREDENTIAL_LIST).get(1), is(CARL));
        assertThat(mModel.get(CREDENTIAL_LIST).get(2), is(BOB));
    }

    @Test
    public void testClearsCredentialListWhenShowingAgain() {
        mMediator.showCredentials(TEST_URL, Collections.singletonList(ANA));
        assertThat(mModel.get(CREDENTIAL_LIST).size(), is(1));
        assertThat(mModel.get(CREDENTIAL_LIST).get(0), is(ANA));

        // Showing the sheet a second time should replace all changed credentials.
        mMediator.showCredentials(TEST_URL, Collections.singletonList(BOB));
        assertThat(mModel.get(CREDENTIAL_LIST).size(), is(1));
        assertThat(mModel.get(CREDENTIAL_LIST).get(0), is(BOB));
    }

    @Test
    public void testShowCredentialsSetsVisibile() {
        mMediator.showCredentials(TEST_URL, Arrays.asList(ANA, CARL, BOB));
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        mMediator.showCredentials(TEST_URL, Arrays.asList(ANA, CARL));
        mMediator.onSelectItemAt(1);
        verify(mMockDelegate).onCredentialSelected(CARL);
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        mMediator.showCredentials(TEST_URL, Arrays.asList(ANA, CARL));
        mMediator.onDismissed();
        verify(mMockDelegate).onDismissed();
        assertThat(mModel.get(VISIBLE), is(false));
    }
}

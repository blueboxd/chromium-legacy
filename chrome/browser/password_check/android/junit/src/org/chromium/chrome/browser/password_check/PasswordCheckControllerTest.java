// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_TIMESTAMP;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.COMPROMISED_CREDENTIALS_COUNT;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.UNKNOWN_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_OFFLINE;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.IDLE;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.RUNNING;

import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Consumer;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_check.PasswordCheckProperties.ItemType;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller tests verify that the PasswordCheck controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.PASSWORD_CHECK)
public class PasswordCheckControllerTest {
    private static final CompromisedCredential ANA =
            new CompromisedCredential("https://m.a.xyz/", "Ana", "password", false, false);
    private static final CompromisedCredential BOB =
            new CompromisedCredential("https://www.b.ch/", "Baub", "DoneSth", false, true);

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    private static final Pair<Integer, Integer> PROGRESS_UPDATE = new Pair<>(2, 19);

    @Mock
    private PasswordCheckComponentUi.Delegate mDelegate;
    @Mock
    private Consumer<String> mLaunchCctWithChangePasswordUrlConsumer;
    @Mock
    private Consumer<CompromisedCredential> mLaunchCctWithScriptConsumer;
    @Mock
    private PasswordCheck mPasswordCheck;

    // DO NOT INITIALIZE HERE! The objects would be shared here which leaks state between tests.
    private PasswordCheckMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = PasswordCheckProperties.createDefaultModel();
        mMediator = new PasswordCheckMediator(
                mLaunchCctWithChangePasswordUrlConsumer, mLaunchCctWithScriptConsumer);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        mMediator.initialize(mModel, mDelegate);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        verify(mPasswordCheck).addObserver(mMediator, true);
        assertNotNull(mModel.get(ITEMS));
    }

    @Test
    public void testAddsAndRemovesFromObserverList() {
        mMediator.destroy();
        verify(mPasswordCheck).removeObserver(mMediator);
    }

    @Test
    public void testInitializeRunningHeader() {
        assertRunningHeader(mModel.get(ITEMS).get(0), UNKNOWN_PROGRESS);
    }

    @Test
    public void testCreatesHeaderForStatus() {
        mMediator.onPasswordCheckStatusChanged(IDLE);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(ITEMS);
        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(CHECK_STATUS), is(IDLE));
    }

    @Test
    public void testUpdateStatusHeaderOnError() {
        assertRunningHeader(mModel.get(ITEMS).get(0), UNKNOWN_PROGRESS);
        mMediator.onPasswordCheckStatusChanged(ERROR_OFFLINE);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(ITEMS);
        assertThat(itemList.size(), is(1));
        MVCListAdapter.ListItem header = itemList.get(0);
        assertHeaderTypeWithStatus(header, ERROR_OFFLINE);
        assertNull(header.model.get(CHECK_PROGRESS));
        assertNull(header.model.get(CHECK_TIMESTAMP));
        assertNull(header.model.get(COMPROMISED_CREDENTIALS_COUNT));
    }

    @Test
    public void testUpdateStatusHeaderOnIdle() {
        assertRunningHeader(mModel.get(ITEMS).get(0), UNKNOWN_PROGRESS);
        mMediator.onPasswordCheckStatusChanged(IDLE);
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(ITEMS);
        assertThat(itemList.size(), is(1));
        assertIdleHeader(itemList.get(0));
    }

    @Test
    public void testUpdateProgressHeader() {
        assertRunningHeader(mModel.get(ITEMS).get(0), UNKNOWN_PROGRESS);
        mMediator.onPasswordCheckProgressChanged(PROGRESS_UPDATE);
        assertRunningHeader(mModel.get(ITEMS).get(0), PROGRESS_UPDATE);
    }

    @Test
    public void testCreatesEntryForExistingCredentials() {
        when(mPasswordCheck.getCompromisedCredentials())
                .thenReturn(new CompromisedCredential[] {ANA});

        mMediator.onPasswordCheckStatusChanged(IDLE);
        mMediator.onCompromisedCredentialsFetchCompleted();

        assertThat(mModel.get(ITEMS).get(1).type, is(ItemType.COMPROMISED_CREDENTIAL));
        assertThat(mModel.get(ITEMS).get(1).model.get(COMPROMISED_CREDENTIAL), equalTo(ANA));
        assertThat(mModel.get(ITEMS).get(1).model.get(CREDENTIAL_HANDLER), is(mMediator));
    }

    @Test
    public void testAppendsEntryForNewlyFoundCredentials() {
        when(mPasswordCheck.getCompromisedCredentials())
                .thenReturn(new CompromisedCredential[] {ANA});
        mMediator.onPasswordCheckStatusChanged(IDLE);
        mMediator.onCompromisedCredentialsFetchCompleted();
        assertThat(mModel.get(ITEMS).size(), is(2)); // Header + existing credentials.

        mMediator.onCompromisedCredentialFound(
                BOB.getOriginUrl(), BOB.getUsername(), BOB.getPassword(), BOB.hasScript());

        assertThat(mModel.get(ITEMS).get(2).type, is(ItemType.COMPROMISED_CREDENTIAL_WITH_SCRIPT));
        assertThat(mModel.get(ITEMS).get(2).model.get(COMPROMISED_CREDENTIAL), equalTo(BOB));
        assertThat(mModel.get(ITEMS).get(2).model.get(CREDENTIAL_HANDLER), is(mMediator));
    }

    @Test
    public void testRemovingElementTriggersDelegate() {
        mMediator.onRemove(ANA);
        verify(mDelegate).removeCredential(eq(ANA));
    }

    @Test
    public void testOnChangePasswordButtonClick() {
        mMediator.onChangePasswordButtonClick(ANA);
        verify(mLaunchCctWithChangePasswordUrlConsumer).accept(eq(ANA.getOriginUrl()));
    }

    @Test
    public void testOnChangePasswordWithScriptButtonClick() {
        mMediator.onChangePasswordWithScriptButtonClick(BOB);
        verify(mLaunchCctWithScriptConsumer).accept(eq(BOB));
    }

    private void assertIdleHeader(MVCListAdapter.ListItem header) {
        assertHeaderTypeWithStatus(header, IDLE);
        assertNull(header.model.get(CHECK_PROGRESS));
        assertNotNull(header.model.get(CHECK_TIMESTAMP));
        assertNotNull(header.model.get(COMPROMISED_CREDENTIALS_COUNT));
    }

    private void assertRunningHeader(
            MVCListAdapter.ListItem header, Pair<Integer, Integer> progress) {
        assertHeaderTypeWithStatus(header, RUNNING);
        assertThat(header.model.get(CHECK_PROGRESS), is(progress));
        assertNull(header.model.get(CHECK_TIMESTAMP));
        assertNull(header.model.get(COMPROMISED_CREDENTIALS_COUNT));
    }

    private void assertHeaderTypeWithStatus(
            MVCListAdapter.ListItem header, @PasswordCheckUIStatus int status) {
        assertThat(header.type, is(ItemType.HEADER));
        assertThat(header.model.get(CHECK_STATUS), is(status));
    }
}

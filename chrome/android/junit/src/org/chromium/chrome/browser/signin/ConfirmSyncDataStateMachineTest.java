// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Matchers;
import org.mockito.Mock;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Tests for {@link ConfirmSyncDataStateMachine}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ConfirmSyncDataStateMachineTest {
    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private SigninManager.Natives mSigninManagerNativeMock;

    @Mock
    private ConfirmSyncDataStateMachineDelegate mDelegateMock;

    @Mock
    private ConfirmSyncDataStateMachine.Listener mStateMachineListenerMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Captor
    private ArgumentCaptor<Callback<Boolean>> mCallbackArgument;

    private final String mOldAccountName = "old.account.test@testdomain.com";

    private final String mNewAccountName = "new.account.test@testdomain.com";

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(SigninManagerJni.TEST_HOOKS, mSigninManagerNativeMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager()).thenReturn(mSigninManagerMock);
    }

    @Test(expected = AssertionError.class)
    public void testNewAccountNameCannotBeEmpty() {
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, mOldAccountName, null, mStateMachineListenerMock);
    }

    @Test
    public void testImportSyncDataDialogShownWhenOldAndNewAccountNamesAreDifferent() {
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, mOldAccountName, mNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock)
                .showConfirmImportSyncDataDialog(any(ConfirmImportSyncDataDialog.Listener.class),
                        eq(mOldAccountName), eq(mNewAccountName));
    }

    @Test
    public void testProgressDialogShownWhenOldAndNewAccountNamesAreEqual() {
        String oldAndNewAccountName = "test.old.new@testdomain.com";
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(mDelegateMock,
                oldAndNewAccountName, oldAndNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock, never())
                .showConfirmImportSyncDataDialog(
                        any(ConfirmImportSyncDataDialog.Listener.class), anyString(), anyString());
        verify(mDelegateMock)
                .showFetchManagementPolicyProgressDialog(
                        any(ConfirmSyncDataStateMachineDelegate.ProgressDialogListener.class));
    }

    @Test
    public void testProgressDialogShownWhenOldAccountNameIsEmpty() {
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, null, mNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock, never())
                .showConfirmImportSyncDataDialog(
                        any(ConfirmImportSyncDataDialog.Listener.class), anyString(), anyString());
        verify(mDelegateMock)
                .showFetchManagementPolicyProgressDialog(
                        any(ConfirmSyncDataStateMachineDelegate.ProgressDialogListener.class));
    }

    @Test
    public void testListenerConfirmedWhenNewAccountIsNotManaged() {
        mockSigninManagerIsAccountManaged(false);
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, null, mNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock).dismissAllDialogs();
        verify(mStateMachineListenerMock).onConfirm(false);
    }

    @Test
    public void testManagedAccountDialogShownWhenNewAccountIsManaged() {
        mockSigninManagerIsAccountManaged(true);
        when(mSigninManagerNativeMock.extractDomainName(anyString())).thenReturn(mNewAccountName);
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, null, mNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock)
                .showSignInToManagedAccountDialog(
                        any(ConfirmManagedSyncDataDialog.Listener.class), eq(mNewAccountName));
    }

    @Test
    public void testWhenManagedAccountStatusIsFetchedAfterNewAccountDialog() {
        String newAccountName = "test.account@manageddomain.com";
        String domain = "manageddomain.com";
        when(mSigninManagerNativeMock.extractDomainName(newAccountName)).thenReturn(domain);
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, null, newAccountName, mStateMachineListenerMock);
        verify(mDelegateMock, never())
                .showSignInToManagedAccountDialog(
                        any(ConfirmManagedSyncDataDialog.Listener.class), anyString());
        verify(mSigninManagerMock)
                .isAccountManaged(eq(newAccountName), mCallbackArgument.capture());
        Callback<Boolean> callback = mCallbackArgument.getValue();
        callback.onResult(true);
        verify(mDelegateMock)
                .showSignInToManagedAccountDialog(
                        any(ConfirmManagedSyncDataDialog.Listener.class), eq(domain));
    }

    @Test
    public void testCancelWhenIsNotBeingDestroyed() {
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, mOldAccountName, mNewAccountName, mStateMachineListenerMock);
        stateMachine.onCancel();
        verify(mStateMachineListenerMock).onCancel();
        verify(mDelegateMock).dismissAllDialogs();
    }

    @Test
    public void testCancelWhenIsBeingDestroyed() {
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, mOldAccountName, mNewAccountName, mStateMachineListenerMock);
        stateMachine.cancel(true);
        verify(mStateMachineListenerMock, never()).onCancel();
        verify(mDelegateMock, never()).dismissAllDialogs();
    }

    @Test(expected = IllegalStateException.class)
    public void testStateCannotChangeOnceDone() {
        ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                mDelegateMock, mOldAccountName, mNewAccountName, mStateMachineListenerMock);
        stateMachine.cancel(true);
        stateMachine.onConfirm();
    }

    private void mockSigninManagerIsAccountManaged(boolean isAccountManaged) {
        doAnswer(invocation -> {
            Callback<Boolean> callback = invocation.getArgument(1);
            callback.onResult(isAccountManaged);
            return null;
        })
                .when(mSigninManagerMock)
                .isAccountManaged(anyString(), Matchers.<Callback<Boolean>>any());
    }
}

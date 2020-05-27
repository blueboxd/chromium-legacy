// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.RootMatchers.isDialog;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * This class regroups the integration tests for {@link ConfirmSyncDataStateMachine}.
 *
 * In this class we use a real {@link ConfirmSyncDataStateMachineDelegate} to walk through
 * different states of the state machine by clicking on the dialogs shown with the delegate.
 * This way we tested the invocation of delegate inside state machine and vice versa.
 *
 * In contrast, {@link ConfirmSyncDataStateMachineTest} takes a delegate mock to check the
 * interaction between the state machine and its delegate in one level.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ConfirmSyncDataIntegrationTest extends DummyUiActivityTestCase {
    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private SigninManager.Natives mSigninManagerNativeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private ConfirmSyncDataStateMachine.Listener mListenerMock;

    private ConfirmSyncDataStateMachineDelegate mDelegate;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(SigninManagerJni.TEST_HOOKS, mSigninManagerNativeMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(IdentityServicesProvider.get().getSigninManager()).thenReturn(mSigninManagerMock);
        mDelegate =
                new ConfirmSyncDataStateMachineDelegate(getActivity().getSupportFragmentManager());
    }

    @Test
    @MediumTest
    public void testNonManagedAccountPositiveButtonInConfirmImportSyncDataDialog() {
        mockSigninManagerIsAccountManaged(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                    mDelegate, "test.account1@gmail.com", "test.account2@gmail.com", mListenerMock);
        });
        onView(withId(R.id.sync_keep_separate_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).perform(click());
        verify(mListenerMock).onConfirm(true);
    }

    @Test
    @MediumTest
    public void testManagedAccountPositiveButtonInConfirmImportSyncDataDialog() {
        mockSigninManagerIsAccountManaged(true);
        String managedNewAccountName = "test.account@manageddomain.com";
        String domain = "manageddomain.com";
        when(mSigninManagerNativeMock.extractDomainName(managedNewAccountName)).thenReturn(domain);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ConfirmSyncDataStateMachine stateMachine = new ConfirmSyncDataStateMachine(
                    mDelegate, "test.account1@gmail.com", managedNewAccountName, mListenerMock);
        });
        onView(withId(R.id.sync_confirm_import_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).perform(click());
        onView(withText(R.string.policy_dialog_proceed)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onConfirm(false);
    }

    private void mockSigninManagerIsAccountManaged(boolean isAccountManaged) {
        doAnswer(invocation -> {
            Callback<Boolean> callback = invocation.getArgument(1);
            callback.onResult(isAccountManaged);
            return null;
        })
                .when(mSigninManagerMock)
                .isAccountManaged(anyString(), any());
    }
}

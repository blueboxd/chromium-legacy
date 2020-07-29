// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.os.Bundle;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_check.PasswordCheck.CheckStatus;
import org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties;
import org.chromium.chrome.browser.password_check.internal.R;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * View tests for the Password Check component ensure model changes are reflected in the check UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordCheckViewTest {
    private static final CompromisedCredential ANA =
            new CompromisedCredential("some-url.com", "Ana", "password", false);
    private static final CompromisedCredential PHISHED =
            new CompromisedCredential("example.com", "Baub", "DoSomething", true);
    private static final CompromisedCredential LEAKED =
            new CompromisedCredential("some-other-url.com", "AZiegler", "N0M3rcy", false);

    private PropertyModel mModel = PasswordCheckProperties.createDefaultModel();
    private PasswordCheckFragmentView mPasswordCheckView;

    @Mock
    private PasswordCheckComponentUi mComponentUi;
    @Mock
    private PasswordCheckCoordinator.CredentialEventHandler mMockHandler;

    @Rule
    public SettingsActivityTestRule<PasswordCheckFragmentView> mTestRule =
            new SettingsActivityTestRule<>(PasswordCheckFragmentView.class);

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        PasswordCheckComponentUiFactory.setCreationStrategy(fragmentView -> {
            mPasswordCheckView = (PasswordCheckFragmentView) fragmentView;
            mPasswordCheckView.setComponentDelegate(mComponentUi);
            return mComponentUi;
        });
        setUpUiLaunchedFromSettings();
        runOnUiThreadBlocking(() -> {
            PasswordCheckCoordinator.setUpModelChangeProcessors(mModel, mPasswordCheckView);
        });
    }

    @Test
    @MediumTest
    public void testDisplaysHeaderAndCredential() {
        runOnUiThreadBlocking(() -> {
            mModel.get(ITEMS).add(buildHeader(CheckStatus.SUCCESS));
            mModel.get(ITEMS).add(buildCredentialItem(ANA));
        });
        pollUiThread(() -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(2)));
        // Has a change passwords button.
        assertNotNull(getCredentialChangeButtonAt(1));
        // TODO(crbug.com/1092444): Ensure the button is visible as soon as it does something!
        assertThat(getCredentialChangeButtonAt(1).getVisibility(), is(View.GONE));
        assertThat(getCredentialChangeButtonAt(1).getText(),
                is(getString(R.string.password_check_credential_row_change_button_caption)));

        // Has a more button.
        assertNotNull(getCredentialMoreButtonAt(1));
        assertThat(getCredentialMoreButtonAt(1).getVisibility(), is(View.VISIBLE));
        assertThat(getCredentialMoreButtonAt(1).getContentDescription(),
                is(getString(org.chromium.chrome.R.string.more)));
    }

    @Test
    @MediumTest
    public void testStatusDisplaysRestartAction() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(CheckStatus.SUCCESS)); });
        pollUiThread(() -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(1)));
        assertThat(getActionButton().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusNotDisplaysRestartAction() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(CheckStatus.RUNNING)); });
        pollUiThread(() -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(1)));
        assertThat(getActionButton().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testCrendentialDisplaysNameOriginAndReason() {
        runOnUiThreadBlocking(() -> {
            mModel.get(ITEMS).add(buildCredentialItem(PHISHED));
            mModel.get(ITEMS).add(buildCredentialItem(LEAKED));
        });
        pollUiThread(() -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(2)));

        // The phished credential is rendered first:
        assertThat(getCredentialOriginAt(0).getText(), is(PHISHED.getOriginUrl()));
        assertThat(getCredentialUserAt(0).getText(), is(PHISHED.getUsername()));
        assertThat(getCredentialReasonAt(0).getText(),
                is(getString(R.string.password_check_credential_row_reason_phished)));

        // The leaked credential is rendered second:
        assertThat(getCredentialOriginAt(1).getText(), is(LEAKED.getOriginUrl()));
        assertThat(getCredentialUserAt(1).getText(), is(LEAKED.getUsername()));
        assertThat(getCredentialReasonAt(1).getText(),
                is(getString(R.string.password_check_credential_row_reason_leaked)));
    }

    @Test
    @MediumTest
    public void testClickingDeleteInMoreMenuTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(ANA)));
        pollUiThread(() -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(1)));

        TouchCommon.singleClickView(getCredentialMoreButtonAt(0));

        onView(withText(org.chromium.chrome.R.string.remove))
                .inRoot(withDecorView(
                        not(is(mPasswordCheckView.getActivity().getWindow().getDecorView()))))
                .perform(click());

        verify(mMockHandler).onRemove(eq(ANA));
    }

    private MVCListAdapter.ListItem buildHeader(@CheckStatus int status) {
        return new MVCListAdapter.ListItem(PasswordCheckProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(CHECK_STATUS, status)
                        .build());
    }

    private MVCListAdapter.ListItem buildCredentialItem(CompromisedCredential credential) {
        return new MVCListAdapter.ListItem(PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
                new PropertyModel
                        .Builder(PasswordCheckProperties.CompromisedCredentialProperties.ALL_KEYS)
                        .with(COMPROMISED_CREDENTIAL, credential)
                        .with(CREDENTIAL_HANDLER, mMockHandler)
                        .build());
    }

    private void setUpUiLaunchedFromSettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PasswordCheckFragmentView.PASSWORD_CHECK_REFERRER,
                PasswordCheckReferrer.PASSWORD_SETTINGS);
        mTestRule.startSettingsActivity(fragmentArgs);
    }

    private View getStatus() {
        return mPasswordCheckView.getListView().getChildAt(0);
    }

    private ImageButton getActionButton() {
        return getStatus().findViewById(R.id.check_status_restart_button);
    }

    private RecyclerView getPasswordCheckViewList() {
        return mPasswordCheckView.getListView();
    }

    private TextView getCredentialOriginAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(R.id.credential_origin);
    }

    private TextView getCredentialUserAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(R.id.compromised_username);
    }

    private TextView getCredentialReasonAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(R.id.compromised_reason);
    }

    private ButtonCompat getCredentialChangeButtonAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(
                R.id.credential_change_button);
    }

    private ListMenuButton getCredentialMoreButtonAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(
                R.id.credential_menu_button);
    }

    private String getString(@IdRes int stringResource) {
        return mTestRule.getActivity().getString(stringResource);
    }
}

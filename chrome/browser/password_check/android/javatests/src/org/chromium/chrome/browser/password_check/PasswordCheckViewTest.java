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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_TIMESTAMP;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.COMPROMISED_CREDENTIALS_COUNT;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.UNKNOWN_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_NO_PASSWORDS;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_OFFLINE;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_QUOTA_LIMIT;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_SIGNED_OUT;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.ERROR_UNKNOWN;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.IDLE;
import static org.chromium.chrome.browser.password_check.PasswordCheckUIStatus.RUNNING;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.Pair;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties;
import org.chromium.chrome.browser.password_check.internal.R;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
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
            new CompromisedCredential("some-url.com", "Ana", "password", false, false);
    private static final CompromisedCredential PHISHED =
            new CompromisedCredential("example.com", "Baub", "DoSomething", true, false);
    private static final CompromisedCredential LEAKED =
            new CompromisedCredential("some-other-url.com", "AZiegler", "N0M3rcy", false, false);
    private static final CompromisedCredential SCRIPTED =
            new CompromisedCredential("script.com", "Charlie", "secret", false, true);

    private static final int LEAKS_COUNT = 2;

    private static final long S_TO_MS = 1000;
    private static final long MIN_TO_MS = 60 * S_TO_MS;
    private static final long H_TO_MS = 60 * MIN_TO_MS;
    private static final long DAY_TO_MS = 24 * H_TO_MS;

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
            mModel.get(ITEMS).add(buildHeader(RUNNING));
            mModel.get(ITEMS).add(buildCredentialItem(ANA));
        });
        waitForListViewToHaveLength(2);
        // Has a change passwords button.
        assertNotNull(getCredentialChangeButtonAt(1));
        assertThat(getCredentialChangeButtonAt(1).getVisibility(), is(View.VISIBLE));
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
    public void testStatusDisplaysIconOnIdleNoLeaks() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> { mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp)); });
        waitForListViewToHaveLength(1);
        assertDisplaysIcon(R.drawable.ic_check_circle_filled_green_24dp);
    }

    @Test
    @MediumTest
    public void testStatusDisplaysIconOnIdleWithLeaks() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> { mModel.get(ITEMS).add(buildHeader(IDLE, LEAKS_COUNT, checkTimestamp)); });
        waitForListViewToHaveLength(1);
        assertDisplaysIcon(org.chromium.chrome.R.drawable.ic_warning_red_24dp);
    }

    @Test
    @MediumTest
    public void testStatusDisplaysIconOnError() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(ERROR_OFFLINE)); });
        waitForListViewToHaveLength(1);
        assertDisplaysIcon(org.chromium.chrome.R.drawable.ic_error_grey800_24dp_filled);
    }

    @Test
    @MediumTest
    public void testStatusDisplaysProgressBarOnRunning() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(RUNNING)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderIcon().getVisibility(), is(View.GONE));
        assertThat(getHeaderProgressBar().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusDisplaysRestartAction() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> { mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp)); });
        waitForListViewToHaveLength(1);
        assertThat(getActionButton().getVisibility(), is(View.VISIBLE));
        assertTrue(getActionButton().isClickable());
    }

    @Test
    @MediumTest
    public void testStatusNotDisplaysRestartAction() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(RUNNING)); });
        waitForListViewToHaveLength(1);
        assertThat(getActionButton().getVisibility(), is(View.GONE));
        assertFalse(getActionButton().isClickable());
    }

    @Test
    @MediumTest
    public void testStatusRunnningText() {
        runOnUiThreadBlocking(
                () -> { mModel.get(ITEMS).add(buildHeader(RUNNING, UNKNOWN_PROGRESS)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(R.string.password_check_status_message_initial_running)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusIdleNoLeaksText() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> { mModel.get(ITEMS).add(buildHeader(IDLE, 0, checkTimestamp)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(R.string.password_check_status_message_idle_no_leaks)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusIdleWithLeaksText() {
        Long checkTimestamp = System.currentTimeMillis();
        runOnUiThreadBlocking(
                () -> { mModel.get(ITEMS).add(buildHeader(IDLE, LEAKS_COUNT, checkTimestamp)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(mPasswordCheckView.getContext().getResources().getQuantityString(
                        R.plurals.password_check_status_message_idle_with_leaks, LEAKS_COUNT,
                        LEAKS_COUNT)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testStatusErrorOfflineText() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(ERROR_OFFLINE)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(R.string.password_check_status_message_error_offline)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorNoPasswordsText() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(ERROR_NO_PASSWORDS)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(R.string.password_check_status_message_error_no_passwords)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorQuotaLimitText() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(ERROR_QUOTA_LIMIT)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(R.string.password_check_status_message_error_quota_limit)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorQuotaLimitAccountCheckText() {
        runOnUiThreadBlocking(
                () -> { mModel.get(ITEMS).add(buildHeader(ERROR_QUOTA_LIMIT_ACCOUNT_CHECK)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(
                        R.string.password_check_status_message_error_quota_limit_account_check)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorSignedOutText() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(ERROR_SIGNED_OUT)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(R.string.password_check_status_message_error_signed_out)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testStatusErrorUnknownText() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildHeader(ERROR_UNKNOWN)); });
        waitForListViewToHaveLength(1);
        assertThat(getHeaderMessage().getText(),
                is(getString(R.string.password_check_status_message_error_unknown)));
        assertThat(getHeaderMessage().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderDescription().getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testCrendentialDisplaysNameOriginAndReason() {
        runOnUiThreadBlocking(() -> {
            mModel.get(ITEMS).add(buildCredentialItem(PHISHED));
            mModel.get(ITEMS).add(buildCredentialItem(LEAKED));
        });
        waitForListViewToHaveLength(2);

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
    public void testCrendentialDisplaysChangeButtonWithScript() {
        runOnUiThreadBlocking(() -> { mModel.get(ITEMS).add(buildCredentialItem(SCRIPTED)); });
        pollUiThread(() -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(1)));

        // Origin and username.
        assertThat(getCredentialOriginAt(0).getText(), is(SCRIPTED.getOriginUrl()));
        assertThat(getCredentialUserAt(0).getText(), is(SCRIPTED.getUsername()));

        // Reason to show credential.
        assertThat(getCredentialReasonAt(0).getText(),
                is(getString(R.string.password_check_credential_row_reason_leaked)));

        // Change button with script.
        assertNotNull(getCredentialChangeButtonWithScriptAt(0));
        assertThat(getCredentialChangeButtonWithScriptAt(0).getText(),
                is(getString(R.string.password_check_credential_row_change_button_caption)));

        // Explanation for change button with script.
        assertNotNull(getCredentialChangeButtonWithScriptExplanationAt(0));
        assertThat(getCredentialChangeButtonWithScriptExplanationAt(0).getText(),
                is(getString(R.string.password_check_credential_row_script_button_explanation)));

        // Change button without script.
        assertNotNull(getCredentialChangeButtonAt(0));
        assertThat(getCredentialChangeButtonAt(0).getText(),
                is(getString(
                        R.string.password_check_credential_row_change_manually_button_caption)));
    }

    @Test
    @MediumTest
    public void testClickingChangePasswordTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(ANA)));
        waitForListViewToHaveLength(1);

        TouchCommon.singleClickView(getCredentialChangeButtonAt(0));

        waitForEvent(mMockHandler).onChangePasswordButtonClick(eq(ANA));
    }

    @Test
    @MediumTest
    public void testClickingChangePasswordWithScriptTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(SCRIPTED)));
        waitForListViewToHaveLength(1);

        TouchCommon.singleClickView(getCredentialChangeButtonWithScriptAt(0));

        waitForEvent(mMockHandler).onChangePasswordWithScriptButtonClick(eq(SCRIPTED));
    }

    @Test
    @MediumTest
    public void testClickingDeleteInMoreMenuTriggersHandler() {
        runOnUiThreadBlocking(() -> mModel.get(ITEMS).add(buildCredentialItem(ANA)));
        waitForListViewToHaveLength(1);

        TouchCommon.singleClickView(getCredentialMoreButtonAt(0));

        onView(withText(org.chromium.chrome.R.string.remove))
                .inRoot(withDecorView(
                        not(is(mPasswordCheckView.getActivity().getWindow().getDecorView()))))
                .perform(click());

        verify(mMockHandler).onRemove(eq(ANA));
    }

    @Test
    @SmallTest
    public void testGetTimestampStrings() {
        Resources res = mPasswordCheckView.getContext().getResources();
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 10 * S_TO_MS), is("Just now"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, MIN_TO_MS), is("1 minute ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 17 * MIN_TO_MS), is("17 minutes ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, H_TO_MS), is("1 hour ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 13 * H_TO_MS), is("13 hours ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, DAY_TO_MS), is("1 day ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 2 * DAY_TO_MS), is("2 days ago"));
        assertThat(PasswordCheckViewBinder.getTimestamp(res, 315 * DAY_TO_MS), is("315 days ago"));
    }

    private MVCListAdapter.ListItem buildHeader(@PasswordCheckUIStatus int status,
            Integer compromisedCredentialsCount, Long checkTimestamp) {
        return buildHeader(status, compromisedCredentialsCount, checkTimestamp, null);
    }

    private MVCListAdapter.ListItem buildHeader(
            @PasswordCheckUIStatus int status, Pair<Integer, Integer> progress) {
        return buildHeader(status, null, null, progress);
    }

    private MVCListAdapter.ListItem buildHeader(@PasswordCheckUIStatus int status) {
        return buildHeader(status, null, null, null);
    }

    private MVCListAdapter.ListItem buildHeader(@PasswordCheckUIStatus int status,
            Integer compromisedCredentialsCount, Long checkTimestamp,
            Pair<Integer, Integer> progress) {
        return new MVCListAdapter.ListItem(PasswordCheckProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(CHECK_PROGRESS, progress)
                        .with(CHECK_STATUS, status)
                        .with(CHECK_TIMESTAMP, checkTimestamp)
                        .with(COMPROMISED_CREDENTIALS_COUNT, compromisedCredentialsCount)
                        .build());
    }

    private MVCListAdapter.ListItem buildCredentialItem(CompromisedCredential credential) {
        return new MVCListAdapter.ListItem(credential.hasScript()
                        ? PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL_WITH_SCRIPT
                        : PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
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

    private void waitForListViewToHaveLength(int length) {
        pollUiThread(
                () -> Criteria.checkThat(getPasswordCheckViewList().getChildCount(), is(length)));
    }

    private void assertDisplaysIcon(int resourceId) {
        assertThat(getHeaderIcon().getVisibility(), is(View.VISIBLE));
        assertThat(getHeaderProgressBar().getVisibility(), is(View.GONE));
        Drawable icon = getHeaderIcon().getDrawable();
        int widthPx = icon.getIntrinsicWidth();
        int heightPx = icon.getIntrinsicHeight();
        assertTrue(getBitmap(
                AppCompatResources.getDrawable(mPasswordCheckView.getContext(), resourceId),
                widthPx, heightPx)
                           .sameAs(getBitmap(icon, widthPx, heightPx)));
    }

    private View getStatus() {
        return mPasswordCheckView.getListView().getChildAt(0);
    }

    private ImageView getHeaderIcon() {
        return getStatus().findViewById(R.id.check_status_icon);
    }

    private ProgressBar getHeaderProgressBar() {
        return getStatus().findViewById(R.id.check_status_progress);
    }

    private TextView getHeaderDescription() {
        return getStatus().findViewById(R.id.check_status_description);
    }

    private TextView getHeaderMessage() {
        return getStatus().findViewById(R.id.check_status_message);
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

    private ButtonCompat getCredentialChangeButtonWithScriptAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(
                R.id.credential_change_button_with_script);
    }

    private TextView getCredentialChangeButtonWithScriptExplanationAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(
                R.id.script_button_explanation);
    }

    private ListMenuButton getCredentialMoreButtonAt(int index) {
        return getPasswordCheckViewList().getChildAt(index).findViewById(
                R.id.credential_menu_button);
    }

    private String getString(@IdRes int stringResource) {
        return mTestRule.getActivity().getString(stringResource);
    }

    private static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private Bitmap getBitmap(Drawable drawable, int widthPx, int heightPx) {
        Bitmap bitmap = Bitmap.createBitmap(widthPx, heightPx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, widthPx, heightPx);
        drawable.draw(canvas);
        return bitmap;
    }
}

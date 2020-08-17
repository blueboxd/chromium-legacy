// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static junit.framework.Assert.assertTrue;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.password_check.PasswordCheckEditFragmentView.EXTRA_COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckEditFragmentView.EXTRA_NEW_PASSWORD;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.os.Bundle;
import android.text.InputType;
import android.widget.EditText;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.url.GURL;

/**
 * View tests for the Password Check Edit screen only.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordCheckEditViewTest {
    private static final CompromisedCredential ANA =
            new CompromisedCredential("https://some-url.com/signin",
                    new GURL("https://some-url.com/"), "Ana", "some-url.com", "Ana", "password",
                    "https://some-url.com/.well-known/change-password", "", false, false);

    private PasswordCheckEditFragmentView mPasswordCheckEditView;

    @Rule
    public SettingsActivityTestRule<PasswordCheckEditFragmentView> mTestRule =
            new SettingsActivityTestRule<>(PasswordCheckEditFragmentView.class);

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        setUpUiLaunchedFromSettings();
    }

    @Test
    @MediumTest
    public void testLoadsCredential() {
        pollUiThread(() -> mPasswordCheckEditView != null);

        EditText origin = mPasswordCheckEditView.getView().findViewById(R.id.site_edit);
        assertNotNull(origin);
        assertNotNull(origin.getText());
        assertNotNull(origin.getText().toString());
        assertThat(origin.getText().toString(), equalTo(ANA.getDisplayOrigin()));

        EditText username = mPasswordCheckEditView.getView().findViewById(R.id.username_edit);
        assertNotNull(username);
        assertNotNull(username.getText());
        assertNotNull(username.getText().toString());
        assertThat(username.getText().toString(), equalTo(ANA.getDisplayUsername()));

        EditText password = mPasswordCheckEditView.getView().findViewById(R.id.password_edit);
        assertNotNull(password);
        assertNotNull(password.getText());
        assertNotNull(password.getText().toString());
        assertThat(password.getText().toString(), equalTo(ANA.getPassword()));
        assertTrue((password.getInputType() & InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD) != 0);
    }

    @Test
    @MediumTest
    public void testSavesCredentialAndChangedPasswordInBundle() {
        pollUiThread(() -> mPasswordCheckEditView != null);

        // Change the password.
        final String newPassword = "NewPassword";
        EditText password = mPasswordCheckEditView.getView().findViewById(R.id.password_edit);
        assertNotNull(password);
        runOnUiThreadBlocking(() -> password.setText(newPassword));

        // Save state (e.g. like happening on destruction).
        Bundle bundle = new Bundle();
        mPasswordCheckEditView.onSaveInstanceState(bundle);

        // Verify the data that reconstructs the page contains all updated information.
        assertTrue(bundle.containsKey(EXTRA_COMPROMISED_CREDENTIAL));
        assertTrue(bundle.containsKey(EXTRA_NEW_PASSWORD));
        assertThat(bundle.getParcelable(EXTRA_COMPROMISED_CREDENTIAL), equalTo(ANA));
        assertThat(bundle.getString(EXTRA_NEW_PASSWORD), equalTo(newPassword));
    }

    private void setUpUiLaunchedFromSettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putParcelable(EXTRA_COMPROMISED_CREDENTIAL, ANA);
        mTestRule.startSettingsActivity(fragmentArgs);
        mPasswordCheckEditView = mTestRule.getFragment();
    }
}

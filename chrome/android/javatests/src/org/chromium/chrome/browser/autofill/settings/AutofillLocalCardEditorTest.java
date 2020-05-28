// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Instrumentation tests for AutofillLocalCardEditor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalCardEditorTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @Rule
    public final SettingsActivityTestRule<AutofillLocalCardEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillLocalCardEditor.class);

    private static final CreditCard SAMPLE_LOCAL_CARD =
            new CreditCard(/* guid= */ "", /* origin= */ "",
                    /* isLocal= */ true, /* isCached= */ false, /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork = */ "visa",
                    /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "", /* serverId= */ "");

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CARD_NICKNAME_MANAGEMENT})
    public void nicknameFieldNotShown_expOff() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getVisibility())
                .isEqualTo(View.GONE);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CARD_NICKNAME_MANAGEMENT})
    public void testNicknameFieldIsShown() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CARD_NICKNAME_MANAGEMENT})
    public void testInvalidNicknameShowsErrorMessage() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(activity.getResources().getString(
                        R.string.autofill_credit_card_editor_invalid_nickname));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CARD_NICKNAME_MANAGEMENT})
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToValid() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(activity.getResources().getString(
                        R.string.autofill_credit_card_editor_invalid_nickname));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText("Valid Nickname");
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError()).isNull();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CARD_NICKNAME_MANAGEMENT})
    public void testNicknameLengthCappedAt25Characters() throws Exception {
        String veryLongNickname = "This is a very very long nickname";
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText(veryLongNickname);
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });

        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString())
                .isEqualTo(veryLongNickname.substring(0, 25));
    }
}

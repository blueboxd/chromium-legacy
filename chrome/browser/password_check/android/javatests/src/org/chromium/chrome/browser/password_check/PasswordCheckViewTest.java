// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.components.embedder_support.util.UrlUtilities.stripScheme;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;

import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View tests for the Password Check component ensure model changes are reflected in the check UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordCheckViewTest {
    private static final CompromisedCredential ANA =
            new CompromisedCredential("https://some-url.com", "Ana", "password", false);

    private PropertyModel mModel = PasswordCheckProperties.createDefaultModel();
    private PasswordCheckFragmentView mPasswordCheckView;
    @Mock
    private PasswordCheckComponentUi mComponentUi;

    @Rule
    public SettingsActivityTestRule<PasswordCheckFragmentView> mTestRule =
            new SettingsActivityTestRule<>(PasswordCheckFragmentView.class);

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFragmentView.setComponentFactory(fragmentView -> {
            mPasswordCheckView = (PasswordCheckFragmentView) fragmentView;
            return mComponentUi;
        });
        mTestRule.startSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordCheckCoordinator.setUpModelChangeProcessors(mModel, mPasswordCheckView);
        });
    }

    @Test
    @MediumTest
    public void testDisplaysHeaderAndCredential() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(ITEMS).add(
                    new MVCListAdapter.ListItem(PasswordCheckProperties.ItemType.HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(CHECK_STATUS, PasswordCheckProperties.CheckStatus.SUCCESS)
                                    .build()));
            mModel.get(ITEMS).add(buildCredentialItem(ANA));
        });
        pollUiThread(
                () -> Criteria.checkThat(mPasswordCheckView.getListView().getChildCount(), is(2)));
        TextView entry = (TextView) mPasswordCheckView.getListView().getChildAt(1);
        assertThat(entry.getText(), is(stripScheme(ANA.getOriginUrl())));
    }

    private static MVCListAdapter.ListItem buildCredentialItem(CompromisedCredential credential) {
        return new MVCListAdapter.ListItem(PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
                new PropertyModel
                        .Builder(PasswordCheckProperties.CompromisedCredentialProperties.ALL_KEYS)
                        .with(COMPROMISED_CREDENTIAL, credential)
                        .build());
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CheckStatus.SUCCESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_check.PasswordCheckProperties.ItemType;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

/**
 * Controller tests verify that the PasswordCheck controller modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PasswordCheckControllerTest {
    private static final CompromisedCredential ANA =
            new CompromisedCredential("Ana", "https://m.a.xyz/");

    private final PasswordCheckMediator mMediator = new PasswordCheckMediator();
    private final PropertyModel mModel = PasswordCheckProperties.createDefaultModel();

    @Before
    public void setUp() {
        mMediator.initialize(mModel);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mModel.get(ITEMS));
    }

    @Test
    public void testCreatesHeaderAndEntryForCredentials() {
        mMediator.onCompromisedCredentialsAvailable(SUCCESS, Arrays.asList(ANA));
        ListModel<MVCListAdapter.ListItem> itemList = mModel.get(ITEMS);
        assertThat(itemList.get(0).type, is(ItemType.HEADER));
        assertThat(itemList.get(0).model.get(CHECK_STATUS), is(SUCCESS));
        assertThat(itemList.get(1).type, is(ItemType.COMPROMISED_CREDENTIAL));
        assertThat(itemList.get(1).model.get(COMPROMISED_CREDENTIAL), is(ANA));
    }
}

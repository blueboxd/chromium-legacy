// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VIEW_EVENT_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import android.graphics.Bitmap;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
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
    private static final String TEST_SUBDOMAIN_URL = "subdomain.example.xyz";
    private static final String TEST_MOBILE_URL = "www.example.xyz";
    private static final Credential ANA =
            new Credential("Ana", "S3cr3t", "Ana", "https://m.a.xyz/", true);
    private static final Credential BOB =
            new Credential("Bob", "*****", "Bob", TEST_SUBDOMAIN_URL, true);
    private static final Credential CARL =
            new Credential("Carl", "G3h3!m", "Carl", TEST_URL, false);
    private static final @Px int DESIRED_FAVICON_SIZE = 64;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private TouchToFillComponent.Delegate mMockDelegate;

    // Can't be local, as it has to be initialized by initMocks.
    @Captor
    private ArgumentCaptor<Callback<Bitmap>> mCallbackArgumentCaptor;

    private final TouchToFillMediator mMediator = new TouchToFillMediator();
    private final PropertyModel mModel = TouchToFillProperties.createDefaultModel(mMediator);

    public TouchToFillControllerTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForDisplayOmitScheme(anyString()))
                .then(inv -> format(inv.getArgument(0)));
    }

    @Before
    public void setUp() {
        mMediator.initialize(mMockDelegate, mModel, DESIRED_FAVICON_SIZE);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mModel.get(SHEET_ITEMS));
        assertNotNull(mModel.get(VIEW_EVENT_LISTENER));
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(mModel.get(FORMATTED_URL), is(nullValue()));
        assertThat(mModel.get(ORIGIN_SECURE), is(false));
    }

    @Test
    public void testShowCredentialsSetsFormattedUrl() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL, BOB));
        assertThat(mModel.get(FORMATTED_URL), is(TEST_URL));
        assertThat(mModel.get(ORIGIN_SECURE), is(true));
    }

    @Test
    public void testShowCredentialsSetsCredentialListAndRequestsFavicons() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL, BOB));
        ListModel<MVCListAdapter.ListItem> credentialList = mModel.get(SHEET_ITEMS);
        // TODO(https://crbug.com/1013209): Simplify this after adding equals to ModelList.
        assertThat(credentialList.size(), is(3));
        assertThat(credentialList.get(0).type, is(ItemType.CREDENTIAL));
        assertThat(credentialList.get(0).model.get(CREDENTIAL), is(ANA));
        assertThat(credentialList.get(0).model.get(FAVICON), is(nullValue()));
        assertThat(credentialList.get(1).type, is(TouchToFillProperties.ItemType.CREDENTIAL));
        assertThat(credentialList.get(1).model.get(CREDENTIAL), is(CARL));
        assertThat(credentialList.get(1).model.get(FAVICON), is(nullValue()));
        assertThat(credentialList.get(2).type, is(TouchToFillProperties.ItemType.CREDENTIAL));
        assertThat(credentialList.get(2).model.get(CREDENTIAL), is(BOB));
        assertThat(credentialList.get(2).model.get(FAVICON), is(nullValue()));

        verify(mMockDelegate).fetchFavicon(eq("https://m.a.xyz/"), eq(DESIRED_FAVICON_SIZE), any());
        verify(mMockDelegate).fetchFavicon(eq(TEST_URL), eq(DESIRED_FAVICON_SIZE), any());
        verify(mMockDelegate).fetchFavicon(eq(TEST_SUBDOMAIN_URL), eq(DESIRED_FAVICON_SIZE), any());
        verify(mMockDelegate).fetchFavicon(eq(BOB.getOriginUrl()), eq(DESIRED_FAVICON_SIZE), any());
    }

    @Test
    public void testFetchFaviconUpdatesModel() {
        mMediator.showCredentials(TEST_URL, true, Collections.singletonList(CARL));
        ListModel<MVCListAdapter.ListItem> credentialList = mModel.get(SHEET_ITEMS);
        assertThat(credentialList.size(), is(1));
        // TODO(https://crbug.com/1013209): Simplify this after adding equals to ModelList.
        assertThat(credentialList.get(0).type, is(TouchToFillProperties.ItemType.CREDENTIAL));
        assertThat(credentialList.get(0).model.get(CREDENTIAL), is(CARL));
        assertThat(credentialList.get(0).model.get(FAVICON), is(nullValue()));

        // ANA and CARL both have TEST_URL as their origin URL
        verify(mMockDelegate)
                .fetchFavicon(
                        eq(TEST_URL), eq(DESIRED_FAVICON_SIZE), mCallbackArgumentCaptor.capture());
        Callback<Bitmap> callback = mCallbackArgumentCaptor.getValue();
        Bitmap bitmap = Bitmap.createBitmap(
                DESIRED_FAVICON_SIZE, DESIRED_FAVICON_SIZE, Bitmap.Config.ARGB_8888);
        callback.onResult(bitmap);
        assertThat(credentialList.get(0).model.get(FAVICON), is(bitmap));
    }

    @Test
    public void testShowCredentialsFormatPslOrigins() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, BOB));
        assertThat(mModel.get(SHEET_ITEMS).size(), is(2));
        assertThat(mModel.get(SHEET_ITEMS).get(0).type, is(ItemType.CREDENTIAL));
        assertThat(mModel.get(SHEET_ITEMS).get(0).model.get(FORMATTED_ORIGIN),
                is(format(ANA.getOriginUrl())));
        assertThat(mModel.get(SHEET_ITEMS).get(1).type, is(ItemType.CREDENTIAL));
        assertThat(mModel.get(SHEET_ITEMS).get(1).model.get(FORMATTED_ORIGIN),
                is(format(BOB.getOriginUrl())));
    }

    @Test
    public void testClearsCredentialListWhenShowingAgain() {
        mMediator.showCredentials(TEST_URL, true, Collections.singletonList(ANA));
        ListModel<MVCListAdapter.ListItem> credentialList = mModel.get(SHEET_ITEMS);
        // TODO(https://crbug.com/1013209): Simplify this after adding equals to ModelList.
        assertThat(credentialList.size(), is(1));
        assertThat(credentialList.get(0).type, is(ItemType.CREDENTIAL));
        assertThat(credentialList.get(0).model.get(CREDENTIAL), is(ANA));
        assertThat(credentialList.get(0).model.get(FAVICON), is(nullValue()));

        // Showing the sheet a second time should replace all changed credentials.
        mMediator.showCredentials(TEST_URL, true, Collections.singletonList(BOB));
        credentialList = mModel.get(SHEET_ITEMS);
        // TODO(https://crbug.com/1013209): Simplify this after adding equals to ModelList.
        assertThat(credentialList.size(), is(1));
        assertThat(credentialList.get(0).type, is(ItemType.CREDENTIAL));
        assertThat(credentialList.get(0).model.get(CREDENTIAL), is(BOB));
        assertThat(credentialList.get(0).model.get(FAVICON), is(nullValue()));
    }

    @Test
    public void testShowCredentialsSetsVisibile() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL, BOB));
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL));
        assertThat(mModel.get(VISIBLE), is(true));
        assertNotNull(mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER));

        mModel.get(SHEET_ITEMS).get(1).model.get(ON_CLICK_LISTENER).onResult(CARL);
        verify(mMockDelegate).onCredentialSelected(CARL);
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        mMediator.showCredentials(TEST_URL, true, Arrays.asList(ANA, CARL));
        mMediator.onDismissed();
        verify(mMockDelegate).onDismissed();
        assertThat(mModel.get(VISIBLE), is(false));
    }

    /**
     * Helper to verify formatted URLs. The real implementation calls {@link UrlFormatter}. It's not
     * useful to actually reimplement the formatter, so just modify the string in a trivial way.
     * @param originUrl A URL {@link String} to "format".
     * @return A "formatted" URL {@link String}.
     */
    private static String format(String originUrl) {
        return "formatted_" + originUrl + "_formatted";
    }
}

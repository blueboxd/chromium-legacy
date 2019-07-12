// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivity;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.RoundedCornerImageView;

/**
 * Tests for RevampedContextMenuHeader view and {@link RevampedContextMenuHeaderViewBinder}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class RevampedContextMenuHeaderViewTest extends DummyUiActivityTestCase {
    private static final String TITLE_STRING = "Some Very Cool Title";
    private static final int TITLE_MAX_COUNT = 2;
    private static final String URL_STRING = "www.website.com";
    private static final int URL_MAX_COUNT = 1;

    private View mHeaderView;
    private TextView mTitle;
    private TextView mUrl;
    private View mTitleAndUrl;
    private RoundedCornerImageView mImage;
    private View mCircleBg;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        DummyUiActivity.setTestLayout(R.layout.revamped_context_menu_header);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mHeaderView = getActivity().findViewById(android.R.id.content);
            mTitle = mHeaderView.findViewById(R.id.menu_header_title);
            mUrl = mHeaderView.findViewById(R.id.menu_header_url);
            mTitleAndUrl = mHeaderView.findViewById(R.id.title_and_url);
            mImage = mHeaderView.findViewById(R.id.menu_header_image);
            mCircleBg = mHeaderView.findViewById(R.id.circle_background);
        });
        mModel = new PropertyModel.Builder(RevampedContextMenuHeaderProperties.ALL_KEYS)
                         .with(RevampedContextMenuHeaderProperties.TITLE, "")
                         .with(RevampedContextMenuHeaderProperties.URL, "")
                         .with(RevampedContextMenuHeaderProperties.URL_CLICK_LISTENER, null)
                         .with(RevampedContextMenuHeaderProperties.IMAGE, null)
                         .with(RevampedContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, false)
                         .build();
        mMCP = PropertyModelChangeProcessor.create(
                mModel, mHeaderView, RevampedContextMenuHeaderViewBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTitle() {
        assertThat(
                "Incorrect initial title visibility.", mTitle.getVisibility(), equalTo(View.GONE));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(RevampedContextMenuHeaderProperties.TITLE, TITLE_STRING);
            mModel.set(RevampedContextMenuHeaderProperties.TITLE_MAX_LINES, TITLE_MAX_COUNT);
        });

        assertThat("Incorrect title visibility.", mTitle.getVisibility(), equalTo(View.VISIBLE));
        assertThat("Incorrect title string.", mTitle.getText(), equalTo(TITLE_STRING));
        assertThat("Incorrect max line count for title.", mTitle.getMaxLines(),
                equalTo(TITLE_MAX_COUNT));
        assertThat("Incorrect title ellipsize mode.", mTitle.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testUrl() {
        assertThat("Incorrect initial URL visibility.", mUrl.getVisibility(), equalTo(View.GONE));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(RevampedContextMenuHeaderProperties.URL, URL_STRING);
            mModel.set(RevampedContextMenuHeaderProperties.URL_MAX_LINES, URL_MAX_COUNT);
        });

        assertThat("Incorrect URL visibility.", mUrl.getVisibility(), equalTo(View.VISIBLE));
        assertThat("Incorrect URL string.", mUrl.getText(), equalTo(URL_STRING));
        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(), equalTo(URL_MAX_COUNT));
        assertThat("Incorrect URL ellipsize mode.", mUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(RevampedContextMenuHeaderProperties.URL_MAX_LINES,
                                Integer.MAX_VALUE));

        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("URL is ellipsized when it shouldn't be.", mUrl.getEllipsize());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testUrlClick() {
        // Even though the click event expands/shrinks the url, the click target is the LinearLayout
        // that contains the title and the url to give the user more area to touch.
        assertFalse("URL has onClickListeners when it shouldn't, yet, have.",
                mTitleAndUrl.hasOnClickListeners());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(RevampedContextMenuHeaderProperties.URL, URL_STRING);
            mModel.set(RevampedContextMenuHeaderProperties.URL_MAX_LINES, URL_MAX_COUNT);
            mModel.set(RevampedContextMenuHeaderProperties.URL_CLICK_LISTENER, (v) -> {
                if (mModel.get(RevampedContextMenuHeaderProperties.URL_MAX_LINES)
                        == Integer.MAX_VALUE) {
                    mModel.set(RevampedContextMenuHeaderProperties.URL_MAX_LINES, URL_MAX_COUNT);
                } else {
                    mModel.set(
                            RevampedContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE);
                }
            });
            mTitleAndUrl.callOnClick();
        });

        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("URL is ellipsized when it shouldn't be.", mUrl.getEllipsize());

        TestThreadUtils.runOnUiThreadBlocking(() -> { mTitleAndUrl.callOnClick(); });

        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(), equalTo(URL_MAX_COUNT));
        assertThat("Incorrect URL ellipsize mode.", mUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testImage() {
        assertThat("Incorrect initial circle background visibility.", mCircleBg.getVisibility(),
                equalTo(View.INVISIBLE));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(RevampedContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, true));
        assertThat("Incorrect circle background visibility.", mCircleBg.getVisibility(),
                equalTo(View.VISIBLE));

        assertNull("Thumbnail drawable isn't null when it should be, initially.",
                mImage.getDrawable());
        final Bitmap bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(RevampedContextMenuHeaderProperties.IMAGE, bitmap));
        assertThat("Incorrect thumbnail bitmap.",
                ((BitmapDrawable) mImage.getDrawable()).getBitmap(), equalTo(bitmap));
    }
}

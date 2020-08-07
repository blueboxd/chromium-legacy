// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link TrendyTermsViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TrendyTermsViewBinderTest extends DummyUiActivityTestCase {
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;
    private ViewLookupCachingFrameLayout mTrendyTermView;
    private ChipView mChipView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTrendyTermView =
                    (ViewLookupCachingFrameLayout) getActivity().getLayoutInflater().inflate(
                            R.layout.trendy_terms_item, null);
            mChipView = (ChipView) mTrendyTermView.fastFindViewById(R.id.trendy_term_chip);

            mModel = new PropertyModel(TrendyTermsProperties.ALL_KEYS);
            mMCP = PropertyModelChangeProcessor.create(
                    mModel, mTrendyTermView, TrendyTermsViewBinder::bind);
        });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testTrendyTermString() {
        String trendyTerm = "Sand Wedge";
        assertEquals("", mChipView.getPrimaryTextView().getText());

        mModel.set(TrendyTermsProperties.TRENDY_TERM_STRING, trendyTerm);

        assertEquals(trendyTerm, mChipView.getPrimaryTextView().getText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTrendyTermOnClickListener() {
        AtomicBoolean trendyTermClicked = new AtomicBoolean();
        trendyTermClicked.set(false);
        mChipView.performClick();
        assertFalse(trendyTermClicked.get());

        mModel.set(TrendyTermsProperties.TRENDY_TERM_ICON_ON_CLICK_LISTENER,
                (View view) -> trendyTermClicked.set(true));

        mChipView.performClick();
        assertTrue(trendyTermClicked.get());
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }
}

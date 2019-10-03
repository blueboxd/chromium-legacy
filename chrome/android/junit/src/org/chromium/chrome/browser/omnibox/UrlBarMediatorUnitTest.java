// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.Selection;
import android.text.SpannableStringBuilder;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

/**
 * Unit tests for {@link UrlBarMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlBarMediatorUnitTest {
    @Mock
    UrlBar.UrlTextChangeListener mMockListener;
    @Mock
    UrlBar.UrlTextChangeListener mAnotherMockListener;

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);
        RecordUserAction.setDisabledForTests(true);
        MockitoAnnotations.initMocks(this);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
        RecordUserAction.setDisabledForTests(false);
    }

    @Test
    public void setUrlData_SendsUpdates() {
        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        UrlBarMediator mediator = new UrlBarMediator(model);

        UrlBarData baseData = UrlBarData.create(
                "http://www.example.com", spannable("www.example.com"), 0, 14, "Blah");
        UrlBarData dataWithDifferentDisplay = UrlBarData.create(
                "http://www.example.com", spannable("www.foo.com"), 0, 11, "Blah");
        UrlBarData dataWithDifferentEditing = UrlBarData.create(
                "http://www.example.com", spannable("www.example.com"), 0, 14, "Bar");

        Assert.assertTrue(mediator.setUrlBarData(baseData, UrlBar.ScrollType.SCROLL_TO_TLD, 4));

        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        Assert.assertTrue(mediator.setUrlBarData(
                dataWithDifferentDisplay, UrlBar.ScrollType.SCROLL_TO_TLD, 4));
        Assert.assertTrue(mediator.setUrlBarData(
                dataWithDifferentEditing, UrlBar.ScrollType.SCROLL_TO_TLD, 4));
        Assert.assertTrue(mediator.setUrlBarData(
                dataWithDifferentEditing, UrlBar.ScrollType.SCROLL_TO_BEGINNING, 4));

        Mockito.verify(observer, Mockito.times(3))
                .onPropertyChanged(model, UrlBarProperties.TEXT_STATE);
    }

    @Test
    public void setUrlData_PreventsDuplicateUpdates() {
        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        UrlBarMediator mediator = new UrlBarMediator(model);

        UrlBarData data1 = UrlBarData.create(
                "http://www.example.com", spannable("www.example.com"), 0, 0, "Blah");
        UrlBarData data2 = UrlBarData.create(
                "http://www.example.com", spannable("www.example.com"), 0, 0, "Blah");

        Assert.assertTrue(mediator.setUrlBarData(data1, UrlBar.ScrollType.SCROLL_TO_TLD, 4));

        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        Assert.assertFalse(mediator.setUrlBarData(data1, UrlBar.ScrollType.SCROLL_TO_TLD, 4));
        Assert.assertFalse(mediator.setUrlBarData(data2, UrlBar.ScrollType.SCROLL_TO_TLD, 4));

        Mockito.verifyZeroInteractions(observer);
    }

    @Test
    public void setUrlData_ScrollStateForDataUrl() {
        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        UrlBarMediator mediator = new UrlBarMediator(model);

        String displayText = "data:text/html,blah";
        UrlBarData data = UrlBarData.create(
                "data:text/html,blah,blah", spannable(displayText), 0, displayText.length(), null);
        Assert.assertTrue(mediator.setUrlBarData(data, UrlBar.ScrollType.SCROLL_TO_TLD,
                UrlBarCoordinator.SelectionState.SELECT_ALL));

        // The scroll state should be overridden to SCROLL_TO_BEGINNING for file-type schemes.
        Assert.assertEquals(UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                model.get(UrlBarProperties.TEXT_STATE).scrollType);
    }

    @Test
    public void setUrlData_ScrollStateForAboutUrl() {
        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        UrlBarMediator mediator = new UrlBarMediator(model);

        String displayText = "about:blank#verylongurl.totallylegit.notsuspicious.url.com";
        UrlBarData data = UrlBarData.create(
                displayText, spannable(displayText), 0, displayText.length(), null);
        Assert.assertTrue(mediator.setUrlBarData(data, UrlBar.ScrollType.SCROLL_TO_TLD,
                UrlBarCoordinator.SelectionState.SELECT_ALL));

        // The scroll state should be overridden to SCROLL_TO_BEGINNING for file-type schemes.
        Assert.assertEquals(UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                model.get(UrlBarProperties.TEXT_STATE).scrollType);
    }

    @Test
    public void urlDataComparison_equals() {
        Assert.assertTrue(UrlBarMediator.isNewTextEquivalentToExistingText(null, null));

        // Empty display text, regardless of spanned state.
        Assert.assertTrue(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, spannable(""), 0, 0, null),
                UrlBarData.create(null, "", 0, 0, null)));

        // No editing text, equal display text
        Assert.assertTrue(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, spannable("Test"), 0, 0, null),
                UrlBarData.create(null, spannable("Test"), 0, 0, null)));

        // Equal display and editing text
        Assert.assertTrue(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, spannable("Test"), 0, 0, "Blah"),
                UrlBarData.create(null, spannable("Test"), 0, 0, "Blah")));

        // Equal complex display text and editing text
        SpannableStringBuilder text1 = spannable("Test");
        text1.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(3), 0, 3, 0);
        text1.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(4), 1, 3, 0);
        text1.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan(), 0, 1, 0);

        SpannableStringBuilder text2 = spannable("Test");
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(3), 0, 3, 0);
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(4), 1, 3, 0);
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan(), 0, 1, 0);

        Assert.assertTrue(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, text1, 0, 0, "Blah"),
                UrlBarData.create(null, text2, 0, 0, "Blah")));

        // Ensure adding non-emphasis spans does not mess up equality.
        text1.setSpan(new Object(), 0, 3, 0);
        Selection.setSelection(text2, 0, 1);
        Assert.assertTrue(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, text1, 0, 0, "Blah"),
                UrlBarData.create(null, text2, 0, 0, "Blah")));
    }

    @Test
    public void urlDataComparison_notEquals() {
        Assert.assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(null, UrlBarData.EMPTY));
        Assert.assertFalse(
                UrlBarMediator.isNewTextEquivalentToExistingText(UrlBarData.EMPTY, null));

        // Different display texts
        Assert.assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, spannable("Test"), 0, 0, null),
                UrlBarData.create(null, spannable("Test2"), 0, 0, null)));

        // Mismatched spannable state of display text
        Assert.assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, spannable("Test"), 0, 0, null),
                UrlBarData.create(null, "Test2", 0, 0, null)));

        // Equal display text, different editing text
        Assert.assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, spannable("Test"), 0, 0, "Blah"),
                UrlBarData.create(null, spannable("Test"), 0, 0, "Blah2")));

        // Equal display text content, but different emphasis spans
        SpannableStringBuilder text1 = spannable("Test");
        SpannableStringBuilder text2 = spannable("Test");
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(3), 0, 3, 0);
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(4), 1, 3, 0);
        text2.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan(), 0, 1, 0);

        Assert.assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, text1, 0, 0, "Blah"),
                UrlBarData.create(null, text2, 0, 0, "Blah")));

        // Add a subset of emphasis spans, but not all.
        text1.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(3), 0, 3, 0);
        text1.setSpan(new OmniboxUrlEmphasizer.UrlEmphasisColorSpan(4), 1, 3, 0);
        Assert.assertFalse(UrlBarMediator.isNewTextEquivalentToExistingText(
                UrlBarData.create(null, text1, 0, 0, "Blah"),
                UrlBarData.create(null, text2, 0, 0, "Blah")));
    }

    @Test
    public void pasteTextValidation() {
        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        UrlBarMediator mediator = new UrlBarMediator(model) {
            @Override
            protected String sanitizeTextForPaste(String text) {
                return text.trim();
            }
        };

        ClipboardManager clipboard =
                (ClipboardManager) RuntimeEnvironment.application.getSystemService(
                        Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(null);
        Assert.assertNull(mediator.getTextToPaste());

        clipboard.setPrimaryClip(ClipData.newPlainText("", ""));
        Assert.assertEquals("", mediator.getTextToPaste());

        clipboard.setPrimaryClip(ClipData.newPlainText("", "test"));
        Assert.assertEquals("test", mediator.getTextToPaste());

        clipboard.setPrimaryClip(ClipData.newPlainText("", "    test     "));
        Assert.assertEquals("test", mediator.getTextToPaste());
    }

    @Test
    public void cutCopyReplacementTextValidation() {
        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        UrlBarMediator mediator = new UrlBarMediator(model);
        String url = "https://www.test.com/blah";
        String displayText = "test.com/blah";
        String editingText = "www.test.com/blah";
        mediator.setUrlBarData(UrlBarData.create(url, displayText, 0, 12, editingText),
                UrlBar.ScrollType.NO_SCROLL, UrlBarCoordinator.SelectionState.SELECT_ALL);

        // Replacement is only valid if selecting the full text.
        Assert.assertNull(mediator.getReplacementCutCopyText(editingText, 1, 2));

        // Editing text will be replaced with the full URL if selecting all of the text.
        Assert.assertEquals(
                url, mediator.getReplacementCutCopyText(editingText, 0, editingText.length()));

        // If selecting just the URL portion of the editing text, it should be replaced with the
        // unformatted URL.
        Assert.assertEquals(
                "https://www.test.com", mediator.getReplacementCutCopyText(editingText, 0, 12));

        // If the path changed in the editing text changed but the domain is untouched, it should
        // be replaced with the full domain from the unformatted URL.
        Assert.assertEquals("https://www.test.com/foo",
                mediator.getReplacementCutCopyText("www.test.com/foo", 0, 16));
    }

    @Test
    public void urlTextChangeListenerCompositeObserver() {
        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        UrlBarMediator mediator = new UrlBarMediator(model);
        mediator.addUrlTextChangeListener(mMockListener);

        String text = "foo";
        String textWithAutocomplete = "foo.bar";
        mediator.onTextChanged(text, textWithAutocomplete);
        Mockito.verify(mMockListener, Mockito.times(1)).onTextChanged(text, textWithAutocomplete);

        mediator.addUrlTextChangeListener(mAnotherMockListener);
        mediator.onTextChanged(text, textWithAutocomplete);
        Mockito.verify(mMockListener, Mockito.times(2)).onTextChanged(text, textWithAutocomplete);
        Mockito.verify(mAnotherMockListener, Mockito.times(1))
                .onTextChanged(text, textWithAutocomplete);
    }

    private static SpannableStringBuilder spannable(String text) {
        return new SpannableStringBuilder(text);
    }
}

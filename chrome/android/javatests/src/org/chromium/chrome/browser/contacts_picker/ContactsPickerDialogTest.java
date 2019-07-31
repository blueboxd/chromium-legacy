// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.ContactsPickerListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for the ContactsPickerDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures({ChromeFeatureList.CONTACTS_PICKER_SELECT_ALL})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContactsPickerDialogTest
        implements ContactsPickerListener, SelectionObserver<ContactDetails> {
    // The timeout (in seconds) to wait for the decoder service to be ready.
    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(30);

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    // The dialog we are testing.
    private ContactsPickerDialog mDialog;

    // The data to show in the dialog.
    private ArrayList<ContactDetails> mTestContacts;

    // The selection delegate for the dialog.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // The last action recorded in the dialog (e.g. contacts selected).
    private @ContactsPickerAction int mLastActionRecorded;

    // The final set of contacts picked by the dialog.
    private List<ContactsPickerListener.Contact> mLastSelectedContacts;

    // The list of currently selected contacts (built piecemeal).
    private List<ContactDetails> mCurrentContactSelection;

    // A callback that fires when something is selected in the dialog.
    public final CallbackHelper onSelectionCallback = new CallbackHelper();

    // A callback that fires when an action is taken in the dialog (cancel/done etc).
    public final CallbackHelper onActionCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestContacts = new ArrayList<ContactDetails>();
        mTestContacts.add(new ContactDetails(
                "0", "Contact 0", Arrays.asList("0@example.com"), Arrays.asList("555-1234")));
        mTestContacts.add(new ContactDetails("1", "Contact 1", null, null));
        mTestContacts.add(new ContactDetails("2", "Contact 2", null, null));
        mTestContacts.add(new ContactDetails("3", "Contact 3", null, null));
        mTestContacts.add(new ContactDetails("4", "Contact 4", null, null));
        mTestContacts.add(new ContactDetails("5", "Contact 5", null, null));
        PickerAdapter.setTestContacts(mTestContacts);

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.BLUE);
        ContactViewHolder.setIconForTesting(bitmap);
    }

    // ContactsPickerDialog.ContactsPickerListener:

    @Override
    public void onContactsPickerUserAction(
            @ContactsPickerAction int action, List<ContactsPickerListener.Contact> contacts) {
        mLastActionRecorded = action;
        mLastSelectedContacts = (contacts != null) ? new ArrayList<>(contacts) : null;
        onActionCallback.notifyCalled();
    }

    // SelectionObserver:

    @Override
    public void onSelectionStateChange(List<ContactDetails> contactsSelected) {
        mCurrentContactSelection = contactsSelected;
        onSelectionCallback.notifyCalled();
    }

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mDialog.findViewById(R.id.recycler_view);
    }

    private ContactsPickerDialog createDialog(final boolean multiselect, final boolean includeNames,
            final boolean includeEmails, final boolean includeTel) throws Exception {
        final ContactsPickerDialog dialog =
                TestThreadUtils.runOnUiThreadBlocking(new Callable<ContactsPickerDialog>() {
                    @Override
                    public ContactsPickerDialog call() {
                        final ContactsPickerDialog dialog =
                                new ContactsPickerDialog(mActivityTestRule.getActivity(),
                                        ContactsPickerDialogTest.this, multiselect, includeNames,
                                        includeEmails, includeTel, "example.com");
                        dialog.show();
                        return dialog;
                    }
                });

        mSelectionDelegate = dialog.getCategoryViewForTesting().getSelectionDelegateForTesting();
        if (!multiselect) mSelectionDelegate.setSingleSelectionMode();
        mSelectionDelegate.addObserver(this);
        mDialog = dialog;

        return dialog;
    }

    /**
     * Clicks a single view in the RecyclerView that occupies slot |position|. Note that even though
     * the first entry in the RecyclerView is a Select All checkbox, this function automatically
     * skips past it. This way, calling clickView for |position| 0 will click the first Contact in
     * the list (and not the Select All checkbox).
     * @param position The position of the item to click (zero-based), or -1 if the intention is to
     *                 toggle the Select All checkbox.
     * @param expectedSelectionCount The expected selection count after the view has been clicked.
     * @param expectSelection True if the clicked-on view should become selected.
     */
    private void clickView(final int position, final int expectedSelectionCount,
            final boolean expectSelection) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.scrollToView(recyclerView, position + 1);

        int callCount = onSelectionCallback.getCallCount();
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                recyclerView.findViewHolderForAdapterPosition(position + 1).itemView);
        onSelectionCallback.waitForCallback(callCount, 1);

        // Validate the correct selection took place.
        if (position != -1) {
            Assert.assertEquals(expectedSelectionCount, mCurrentContactSelection.size());
            Assert.assertEquals(expectSelection,
                    mSelectionDelegate.isItemSelected(mTestContacts.get(position)));
        }
    }

    private void clickDone() throws Exception {
        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        ContactsPickerToolbar toolbar =
                (ContactsPickerToolbar) mDialog.findViewById(R.id.action_bar);
        Button done = (Button) toolbar.findViewById(R.id.done);
        int callCount = onActionCallback.getCallCount();
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(), done);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
    }

    public void clickCancel() throws Exception {
        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        View cancel = new View(mActivityTestRule.getActivity());
        int callCount = onActionCallback.getCallCount();
        categoryView.onClick(cancel);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(ContactsPickerAction.CANCEL, mLastActionRecorded);
    }

    private void toggleSelectAll(final int expectedSelectionCount,
            final @ContactsPickerAction int expectedAction) throws Exception {
        mLastActionRecorded = ContactsPickerAction.NUM_ENTRIES;

        int callCount = onActionCallback.getCallCount();

        // The clickView function automatically skips the Select All checkbox, which is the first
        // item in the list. Compensate for that by passing in -1.
        clickView(-1, expectedSelectionCount,
                expectedAction == ContactsPickerAction.CONTACTS_SELECTED);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(expectedSelectionCount, mSelectionDelegate.getSelectedItems().size());
        Assert.assertEquals(expectedAction, mLastActionRecorded);
    }

    private void notifyChipToggled(@PickerAdapter.FilterType int filter) {
        TopView topView = mDialog.getCategoryViewForTesting().getTopViewForTesting();
        topView.notifyChipToggled(filter);
    }

    private void toggleFilter(@PickerAdapter.FilterType int filter) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, 0);

        TestThreadUtils.runOnUiThreadBlocking(() -> notifyChipToggled(filter));
    }

    private void clickSearchButton() throws Exception {
        ContactsPickerToolbar toolbar =
                (ContactsPickerToolbar) mDialog.findViewById(R.id.action_bar);
        View search = toolbar.findViewById(R.id.search);
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(), search);
    }

    private void dismissDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mDialog.dismiss());
    }

    private TopView getTopView() throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, 0);
        View view = recyclerView.getLayoutManager().findViewByPosition(0);
        Assert.assertNotNull(view);
        Assert.assertTrue(view instanceof TopView);

        return (TopView) view;
    }

    @Test
    @LargeTest
    public void testOriginString() throws Throwable {
        createDialog(/* multiselect = */ true, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        TopView topView = getTopView();
        Assert.assertNotNull(topView);

        TextView explanation = (TextView) topView.findViewById(R.id.explanation);
        Assert.assertNotNull(explanation);
        Assert.assertEquals(explanation.getText().toString(),
                "The contacts you select below will be shared with the website example.com.");

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testFilterVisibilityForDataInclusion() throws Throwable {
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ false,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        TopView topView = getTopView();
        Assert.assertNotNull(topView);

        View namesFilter = topView.findViewById(R.id.names_filter);
        Assert.assertNotNull(namesFilter);
        View emailFilter = topView.findViewById(R.id.email_filter);
        Assert.assertNotNull(emailFilter);
        View telFilter = topView.findViewById(R.id.tel_filter);
        Assert.assertNotNull(telFilter);

        // Per configuration given in the createDialog() call, the names and telephone filters
        // should be visible, but the e-mail filter should be gone.
        Assert.assertEquals(namesFilter.getVisibility(), View.VISIBLE);
        Assert.assertEquals(emailFilter.getVisibility(), View.GONE);
        Assert.assertEquals(telFilter.getVisibility(), View.VISIBLE);
    }

    @Test
    @LargeTest
    public void testNoSelection() throws Throwable {
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickCancel();

        Assert.assertEquals(null, mLastSelectedContacts);
        Assert.assertEquals(ContactsPickerAction.CANCEL, mLastActionRecorded);

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testSingleSelectionContacts() throws Throwable {
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        // Expected selection count is 1 because clicking on a new view deselects other.
        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickView(1, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(1).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testMultiSelectionContacts() throws Throwable {
        createDialog(/* multiselect = */ true, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        // Multi-selection is enabled, so each click is counted.
        int expectedSelectionCount = 0;
        clickView(0, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(2, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(4, ++expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(3, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(4).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(
                mTestContacts.get(2).getDisplayName(), mLastSelectedContacts.get(1).names.get(0));
        Assert.assertEquals(
                mTestContacts.get(0).getDisplayName(), mLastSelectedContacts.get(2).names.get(0));

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testNamesRemoved() throws Throwable {
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        toggleFilter(PickerAdapter.FilterType.NAMES);

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(new ArrayList<String>(), mLastSelectedContacts.get(0).names);
        Assert.assertEquals(mTestContacts.get(0).getEmails().get(0),
                mLastSelectedContacts.get(0).emails.get(0));

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testEmailsRemoved() throws Throwable {
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        toggleFilter(PickerAdapter.FilterType.EMAILS);

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(0).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(new ArrayList<String>(), mLastSelectedContacts.get(0).emails);

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testTelephonesRemoved() throws Throwable {
        createDialog(/* multiselect = */ false, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        toggleFilter(PickerAdapter.FilterType.TELEPHONES);

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount, /* expectSelection = */ true);
        clickDone();

        Assert.assertEquals(ContactsPickerAction.CONTACTS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(1, mLastSelectedContacts.size());
        Assert.assertEquals(
                mTestContacts.get(0).getDisplayName(), mLastSelectedContacts.get(0).names.get(0));
        Assert.assertEquals(new ArrayList<String>(), mLastSelectedContacts.get(0).tel);

        dismissDialog();
    }

    @Test
    @LargeTest
    public void testSelectAll() throws Throwable {
        createDialog(/* multiselect = */ true, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        toggleSelectAll(6, ContactsPickerAction.SELECT_ALL);
        toggleSelectAll(0, ContactsPickerAction.UNDO_SELECT_ALL);

        // Manually select one item.
        clickView(0, /* expectedSelectionCount = */ 1, /* expectSelection = */ true);

        toggleSelectAll(6, ContactsPickerAction.SELECT_ALL);
        toggleSelectAll(0, ContactsPickerAction.UNDO_SELECT_ALL);

        // Select the rest of the items manually.
        int expectedSelectionCount = 0;
        clickView(1, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(2, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(3, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(4, ++expectedSelectionCount, /* expectSelection = */ true);
        clickView(5, ++expectedSelectionCount, /* expectSelection = */ true);

        toggleSelectAll(6, ContactsPickerAction.SELECT_ALL);
        toggleSelectAll(0, ContactsPickerAction.UNDO_SELECT_ALL);
    }

    @Test
    @LargeTest
    public void testNoSearchStringNoCrash() throws Throwable {
        createDialog(/* multiselect = */ true, /* includeNames = */ true,
                /* includeEmails = */ true,
                /* includeTel = */ true);
        Assert.assertTrue(mDialog.isShowing());

        clickSearchButton();
        dismissDialog();
    }
}

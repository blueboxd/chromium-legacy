// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.ContentResolver;
import android.support.annotation.IntDef;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Locale;

/**
 * A data adapter for the Contacts Picker.
 */
public class PickerAdapter extends Adapter<RecyclerView.ViewHolder>
        implements ContactsFetcherWorkerTask.ContactsRetrievedCallback,
                   TopView.ChipToggledCallback {
    /**
     * A ViewHolder for the top-most view in the RecyclerView. The view it contains has a
     * checkbox and some multi-line text that goes with it, so clicks on either text line
     * should be treated as clicks for the checkbox (hence the onclick forwarding).
     */
    class TopViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
        TopView mItemView;

        public TopViewHolder(TopView itemView) {
            super(itemView);
            mItemView = itemView;
            mItemView.setOnClickListener(this);
        }

        @Override
        public void onClick(View view) {
            // TODO(finnur): Make the explanation text non-clickable.
            mItemView.toggle();
        }
    }

    /**
     * The types of filters supported.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FilterType.NAMES, FilterType.EMAILS, FilterType.TELEPHONES})
    public @interface FilterType {
        int NAMES = 0;
        int EMAILS = 1;
        int TELEPHONES = 2;
    }

    /**
     * The types of views supported.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewType.SELECT_ALL_CHECKBOX, ViewType.CONTACT_DETAILS})
    private @interface ViewType {
        int SELECT_ALL_CHECKBOX = 0;
        int CONTACT_DETAILS = 1;
    }

    // The category view to use to show the contacts.
    private PickerCategoryView mCategoryView;

    // The view at the top of the RecyclerView (disclaimer and select all functionality).
    private TopView mTopView;

    // The origin the data will be shared with, formatted for display with the scheme omitted.
    private final String mFormattedOrigin;

    // The content resolver to query data from.
    private ContentResolver mContentResolver;

    // The full list of all registered contacts on the device.
    private ArrayList<ContactDetails> mContactDetails;

    // The async worker task to use for fetching the contact details.
    private ContactsFetcherWorkerTask mWorkerTask;

    // Whether the user has switched to search mode.
    private boolean mSearchMode;

    // A list of search result indices into the larger data set.
    private ArrayList<Integer> mSearchResults;

    // Whether to include names in the returned results.
    private static boolean sIncludeNames;

    // Whether to include emails in the returned results.
    private static boolean sIncludeEmails;

    // Whether to include telephone numbers in the returned results.
    private static boolean sIncludeTelephones;

    // A list of contacts to use for testing (instead of querying Android).
    private static ArrayList<ContactDetails> sTestContacts;

    /**
     * The PickerAdapter constructor.
     * @param categoryView The category view to use to show the contacts.
     * @param contentResolver The content resolver to use to fetch the data.
     * @param formattedOrigin The origin the data will be shared with.
     */
    public PickerAdapter(PickerCategoryView categoryView, ContentResolver contentResolver,
            String formattedOrigin) {
        mCategoryView = categoryView;
        mContentResolver = contentResolver;
        mFormattedOrigin = formattedOrigin;
        sIncludeNames = true;
        sIncludeEmails = true;
        sIncludeTelephones = true;

        if (getAllContacts() == null && sTestContacts == null) {
            mWorkerTask = new ContactsFetcherWorkerTask(mContentResolver, this,
                    mCategoryView.includeNames, mCategoryView.includeEmails,
                    mCategoryView.includeTel);
            mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } else {
            mContactDetails = sTestContacts;
            notifyDataSetChanged();
        }
    }

    /**
     * Set whether the user has switched to search mode.
     * @param searchMode True when we are in search mode.
     */
    public void setSearchMode(boolean searchMode) {
        mSearchMode = searchMode;
        notifyDataSetChanged();
    }

    /**
     * Sets the search query (filter) for the contact list. Filtering is by display name.
     * @param query The search term to use.
     */
    public void setSearchString(String query) {
        if (query.equals("")) {
            if (mSearchResults == null) return;
            mSearchResults.clear();
            mSearchResults = null;
        } else {
            mSearchResults = new ArrayList<Integer>();
            Integer count = 0;
            String query_lower = query.toLowerCase(Locale.getDefault());
            for (ContactDetails contact : mContactDetails) {
                if (contact.getDisplayName().toLowerCase(Locale.getDefault()).contains(query_lower)
                        || contact.getContactDetailsAsString(
                                          /*longVersion=*/true, includesEmails(),
                                          includesTelephones(), /*resources=*/null)
                                   .toLowerCase(Locale.getDefault())
                                   .contains(query_lower)) {
                    mSearchResults.add(count);
                }
                count++;
            }
        }
        notifyDataSetChanged();
    }

    /**
     * Fetches all known contacts.
     * @return The contact list as an array.
     */
    public ArrayList<ContactDetails> getAllContacts() {
        return mContactDetails;
    }

    // ContactsFetcherWorkerTask.ContactsRetrievedCallback:

    @Override
    public void contactsRetrieved(ArrayList<ContactDetails> contacts) {
        mContactDetails = contacts;
        if (mTopView != null) mTopView.updateContactCount(mContactDetails.size());
        notifyDataSetChanged();
    }

    // RecyclerView.Adapter:

    @Override
    public int getItemViewType(int position) {
        if (position == 0 && !mSearchMode) return ViewType.SELECT_ALL_CHECKBOX;
        return ViewType.CONTACT_DETAILS;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        switch (viewType) {
            case ViewType.SELECT_ALL_CHECKBOX: {
                mTopView = (TopView) LayoutInflater.from(parent.getContext())
                                   .inflate(R.layout.top_view, parent, false);
                mTopView.setSiteString(mFormattedOrigin);
                mTopView.registerSelectAllCallback(mCategoryView);
                mTopView.registerChipToggledCallback(this);
                mTopView.updateCheckboxVisibility(mCategoryView.multiSelectionAllowed());
                mTopView.updateChipVisibility(mCategoryView.includeNames,
                        mCategoryView.includeEmails, mCategoryView.includeTel);
                mCategoryView.setTopView(mTopView);
                if (mContactDetails != null) mTopView.updateContactCount(mContactDetails.size());
                return new TopViewHolder(mTopView);
            }
            case ViewType.CONTACT_DETAILS: {
                ContactView itemView = (ContactView) LayoutInflater.from(parent.getContext())
                                               .inflate(R.layout.contact_view, parent, false);
                itemView.setCategoryView(mCategoryView);
                return new ContactViewHolder(itemView, mCategoryView, mContentResolver);
            }
        }
        return null;
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        switch (holder.getItemViewType()) {
            case ViewType.SELECT_ALL_CHECKBOX:
                // There's no need to bind the Select All view.
                return;
            case ViewType.CONTACT_DETAILS:
                ContactViewHolder contactHolder = (ContactViewHolder) holder;
                ContactDetails contact;
                if (!mSearchMode || mSearchResults == null) {
                    // Subtract one because the first view is the Select All checkbox when not in
                    // search mode.
                    contact = mContactDetails.get(position - (mSearchMode ? 0 : 1));
                } else {
                    Integer index = mSearchResults.get(position);
                    contact = mContactDetails.get(index);
                }

                contactHolder.setContactDetails(contact);
        }
    }

    @Override
    public int getItemCount() {
        if (mSearchResults != null) return mSearchResults.size();
        if (mContactDetails == null || mContactDetails.size() == 0) return 0;
        // Add one entry to account for the Select All checkbox, when not searching.
        return mContactDetails.size() + (mSearchMode ? 0 : 1);
    }

    // TopView.ChipToggledCallback:

    @Override
    public void onChipToggled(@FilterType int chip) {
        switch (chip) {
            case FilterType.NAMES:
                sIncludeNames = !sIncludeNames;
                break;
            case FilterType.EMAILS:
                sIncludeEmails = !sIncludeEmails;
                break;
            case FilterType.TELEPHONES:
                sIncludeTelephones = !sIncludeTelephones;
                break;
            default:
                assert false;
        }

        notifyDataSetChanged();
    }

    /**
     * Returns true unless the adapter is filtering out names.
     */
    public static boolean includesNames() {
        return sIncludeNames;
    }

    /**
     * Returns true unless the adapter is filtering out emails.
     */
    public static boolean includesEmails() {
        return sIncludeEmails;
    }

    /**
     * Returns true unless the adapter is filtering out telephone numbers.
     */
    public static boolean includesTelephones() {
        return sIncludeTelephones;
    }

    /** Sets a list of contacts to use as data for the dialog. For testing use only. */
    @VisibleForTesting
    public static void setTestContacts(ArrayList<ContactDetails> contacts) {
        sTestContacts = contacts;
    }
}

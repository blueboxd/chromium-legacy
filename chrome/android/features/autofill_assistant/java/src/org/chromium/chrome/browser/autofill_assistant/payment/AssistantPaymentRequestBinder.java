// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;

import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestTermsSection.Delegate;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillPaymentApp;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.BasicCardUtils;
import org.chromium.chrome.browser.payments.CardEditor;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.payments.PaymentInstrument;
import org.chromium.chrome.browser.widget.prefeditor.EditorDialog;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class is responsible for pushing updates to the Autofill Assistant payment request. These
 * updates are pulled from the {@link AssistantPaymentRequestModel} when a notification of an update
 * is received.
 */
class AssistantPaymentRequestBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantPaymentRequestModel,
                AssistantPaymentRequestBinder.ViewHolder, PropertyKey> {
    /**
     * Helper class that compares instances of {@link PersonalDataManager.AutofillProfile} by
     * completeness in regards to the current state of the given {@link
     * AssistantPaymentRequestModel}.
     */
    class CompletenessComparator implements Comparator<PersonalDataManager.AutofillProfile> {
        final boolean mRequestName;
        final boolean mRequestShipping;
        final boolean mRequestEmail;
        final boolean mRequestPhone;
        CompletenessComparator(AssistantPaymentRequestModel model) {
            mRequestName = model.get(AssistantPaymentRequestModel.REQUEST_NAME);
            mRequestShipping = model.get(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS);
            mRequestEmail = model.get(AssistantPaymentRequestModel.REQUEST_EMAIL);
            mRequestPhone = model.get(AssistantPaymentRequestModel.REQUEST_PHONE);
        }

        @Override
        public int compare(
                PersonalDataManager.AutofillProfile a, PersonalDataManager.AutofillProfile b) {
            int result = countCompleteFields(a) - countCompleteFields(b);
            // Fallback, compare by name alphabetically.
            if (result == 0) {
                return a.getFullName().compareTo(b.getFullName());
            }
            return result;
        }

        private int countCompleteFields(PersonalDataManager.AutofillProfile profile) {
            int completeFields = 0;
            if (mRequestName && !TextUtils.isEmpty((profile.getFullName()))) {
                completeFields++;
            }
            if (mRequestShipping && !TextUtils.isEmpty(profile.getStreetAddress())) {
                completeFields++;
            }
            if (mRequestEmail && !TextUtils.isEmpty(profile.getEmailAddress())) {
                completeFields++;
            }
            if (mRequestPhone && !TextUtils.isEmpty(profile.getPhoneNumber())) {
                completeFields++;
            }
            return completeFields;
        }
    }

    /**
     * A wrapper class that holds the different views of the payment request.
     */
    static class ViewHolder {
        private final View mRootView;
        private final AssistantVerticalExpanderAccordion mPaymentRequestExpanderAccordion;
        private final int mSectionToSectionPadding;
        private final AssistantPaymentRequestContactDetailsSection mContactDetailsSection;
        private final AssistantPaymentRequestPaymentMethodSection mPaymentMethodSection;
        private final AssistantPaymentRequestShippingAddressSection mShippingAddressSection;
        private final AssistantPaymentRequestTermsSection mTermsSection;
        private final AssistantPaymentRequestTermsSection mTermsAsCheckboxSection;
        private final Object mDividerTag;
        private final Activity mActivity;
        private PersonalDataManager.PersonalDataManagerObserver mPersonalDataManagerObserver;

        public ViewHolder(View rootView, AssistantVerticalExpanderAccordion accordion,
                int sectionPadding,
                AssistantPaymentRequestContactDetailsSection contactDetailsSection,
                AssistantPaymentRequestPaymentMethodSection paymentMethodSection,
                AssistantPaymentRequestShippingAddressSection shippingAddressSection,
                AssistantPaymentRequestTermsSection termsSection,
                AssistantPaymentRequestTermsSection termsAsCheckboxSection, Object dividerTag,
                Activity activity) {
            mRootView = rootView;
            mPaymentRequestExpanderAccordion = accordion;
            mSectionToSectionPadding = sectionPadding;
            mContactDetailsSection = contactDetailsSection;
            mPaymentMethodSection = paymentMethodSection;
            mShippingAddressSection = shippingAddressSection;
            mTermsSection = termsSection;
            mTermsAsCheckboxSection = termsAsCheckboxSection;
            mDividerTag = dividerTag;
            mActivity = activity;
        }

        /**
         * Explicitly clean up.
         */
        public void destroy() {
            stopListenToPersonalDataManager();
        }

        private void startListenToPersonalDataManager(
                PersonalDataManager.PersonalDataManagerObserver observer) {
            if (mPersonalDataManagerObserver != null) {
                return;
            }
            mPersonalDataManagerObserver = observer;
            PersonalDataManager.getInstance().registerDataObserver(mPersonalDataManagerObserver);
        }

        private void stopListenToPersonalDataManager() {
            if (mPersonalDataManagerObserver == null) {
                return;
            }
            PersonalDataManager.getInstance().unregisterDataObserver(mPersonalDataManagerObserver);
            mPersonalDataManagerObserver = null;
        }
    }

    @Override
    public void bind(AssistantPaymentRequestModel model, ViewHolder view, PropertyKey propertyKey) {
        boolean handled = updateEditors(model, propertyKey, view);
        handled = updateRootVisibility(model, propertyKey, view) || handled;
        handled = updateSectionVisibility(model, propertyKey, view) || handled;
        handled = updateSectionContents(model, propertyKey, view) || handled;
        handled = updateSectionSelectedItem(model, propertyKey, view) || handled;
        /* Update section paddings *after* updating section visibility. */
        handled = updateSectionPaddings(model, propertyKey, view) || handled;

        if (propertyKey == AssistantPaymentRequestModel.DELEGATE) {
            AssistantPaymentRequestDelegate delegate =
                    model.get(AssistantPaymentRequestModel.DELEGATE);

            Delegate termsDelegate = delegate == null ? null : new Delegate() {
                @Override
                public void onStateChanged(@AssistantTermsAndConditionsState int state) {
                    delegate.onTermsAndConditionsChanged(state);
                }

                @Override
                public void onLinkClicked(int link) {
                    delegate.onTermsAndConditionsLinkClicked(link);
                }
            };
            view.mTermsSection.setDelegate(termsDelegate);
            view.mTermsAsCheckboxSection.setDelegate(termsDelegate);
            view.mContactDetailsSection.setListener(
                    delegate != null ? delegate::onContactInfoChanged : null);
            view.mPaymentMethodSection.setListener(
                    delegate != null ? delegate::onPaymentMethodChanged : null);
            view.mShippingAddressSection.setListener(
                    delegate != null ? delegate::onShippingAddressChanged : null);
        } else if (propertyKey == AssistantPaymentRequestModel.SUPPORTED_BASIC_CARD_NETWORKS) {
            updateAvailablePaymentMethods(model);
        } else if (propertyKey == AssistantPaymentRequestModel.SUPPORTED_PAYMENT_METHODS) {
            updateAvailableAutofillPaymentMethods(model);
        } else {
            assert handled : "Unhandled property detected in AssistantPaymentRequestBinder!";
        }
    }

    private boolean shouldShowContactDetails(AssistantPaymentRequestModel model) {
        return model.get(AssistantPaymentRequestModel.REQUEST_NAME)
                || model.get(AssistantPaymentRequestModel.REQUEST_PHONE)
                || model.get(AssistantPaymentRequestModel.REQUEST_EMAIL);
    }

    /**
     * Updates the available items for each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionContents(
            AssistantPaymentRequestModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantPaymentRequestModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS) {
            List<AutofillPaymentInstrument> availablePaymentMethods =
                    model.get(AssistantPaymentRequestModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS);
            if (availablePaymentMethods == null) availablePaymentMethods = Collections.emptyList();
            view.mPaymentMethodSection.onAvailablePaymentMethodsChanged(availablePaymentMethods);
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.AVAILABLE_PROFILES) {
            List<PersonalDataManager.AutofillProfile> autofillProfiles =
                    model.get(AssistantPaymentRequestModel.AVAILABLE_PROFILES);
            if (autofillProfiles == null) {
                autofillProfiles = Collections.emptyList();
            }
            if (shouldShowContactDetails(model)) {
                view.mContactDetailsSection.onProfilesChanged(autofillProfiles,
                        model.get(AssistantPaymentRequestModel.REQUEST_EMAIL),
                        model.get(AssistantPaymentRequestModel.REQUEST_NAME),
                        model.get(AssistantPaymentRequestModel.REQUEST_PHONE));
            }
            if (model.get(AssistantPaymentRequestModel.REQUEST_PAYMENT)) {
                view.mPaymentMethodSection.onProfilesChanged(autofillProfiles);
            }
            if (model.get(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS)) {
                view.mShippingAddressSection.onProfilesChanged(autofillProfiles);
            }
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.REQUIRE_BILLING_POSTAL_CODE
                || propertyKey == AssistantPaymentRequestModel.BILLING_POSTAL_CODE_MISSING_TEXT) {
            view.mPaymentMethodSection.setRequiresBillingPostalCode(
                    model.get(AssistantPaymentRequestModel.REQUIRE_BILLING_POSTAL_CODE));
            view.mPaymentMethodSection.setBillingPostalCodeMissingText(
                    model.get(AssistantPaymentRequestModel.BILLING_POSTAL_CODE_MISSING_TEXT));
            return true;
        }

        return false;
    }

    /**
     * Updates visibility of PR sections.
     * @return whether the property key was handled.
     */
    private boolean updateSectionVisibility(
            AssistantPaymentRequestModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey == AssistantPaymentRequestModel.REQUEST_NAME)
                || (propertyKey == AssistantPaymentRequestModel.REQUEST_EMAIL)
                || (propertyKey == AssistantPaymentRequestModel.REQUEST_PHONE)) {
            view.mContactDetailsSection.setVisible(shouldShowContactDetails(model));
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS) {
            view.mShippingAddressSection.setVisible(
                    model.get(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS));
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.REQUEST_PAYMENT) {
            view.mPaymentMethodSection.setVisible(
                    (model.get(AssistantPaymentRequestModel.REQUEST_PAYMENT)));
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT) {
            view.mTermsSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantPaymentRequestModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            view.mTermsAsCheckboxSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantPaymentRequestModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.SHOW_TERMS_AS_CHECKBOX) {
            if (model.get(AssistantPaymentRequestModel.SHOW_TERMS_AS_CHECKBOX)) {
                view.mTermsSection.getView().setVisibility(View.GONE);
                view.mTermsAsCheckboxSection.getView().setVisibility(View.VISIBLE);
            } else {
                view.mTermsSection.getView().setVisibility(View.VISIBLE);
                view.mTermsAsCheckboxSection.getView().setVisibility(View.GONE);
            }
            return true;
        }
        return false;
    }

    /**
     * Updates visibility of the root widget.
     * @return whether the property key was handled.
     */
    private boolean updateRootVisibility(
            AssistantPaymentRequestModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey != AssistantPaymentRequestModel.VISIBLE) {
            return false;
        }
        int visibility = model.get(AssistantPaymentRequestModel.VISIBLE) ? View.VISIBLE : View.GONE;
        if (view.mRootView.getVisibility() != visibility) {
            if (visibility == View.VISIBLE) {
                // Update available profiles and credit cards before PR is made visible.
                updateAvailableProfiles(model, view);
                updateAvailablePaymentMethods(model);
                WebContents webContents = model.get(AssistantPaymentRequestModel.WEB_CONTENTS);
                if (webContents != null) {
                    view.mTermsSection.setOrigin(UrlFormatter.formatUrlForSecurityDisplayOmitScheme(
                            webContents.getLastCommittedUrl()));
                    view.mTermsAsCheckboxSection.setOrigin(
                            UrlFormatter.formatUrlForSecurityDisplayOmitScheme(
                                    webContents.getLastCommittedUrl()));
                }
                view.startListenToPersonalDataManager(() -> {
                    AssistantPaymentRequestBinder.this.updateAvailableProfiles(model, view);
                    AssistantPaymentRequestBinder.this.updateAvailablePaymentMethods(model);
                });
            } else {
                view.stopListenToPersonalDataManager();
            }
            view.mRootView.setVisibility(visibility);
        }
        return true;
    }

    /**
     * Updates the currently selected item in each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionSelectedItem(
            AssistantPaymentRequestModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantPaymentRequestModel.SHIPPING_ADDRESS) {
            view.mShippingAddressSection.addOrUpdateItem(
                    model.get(AssistantPaymentRequestModel.SHIPPING_ADDRESS), true);
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.PAYMENT_METHOD) {
            view.mPaymentMethodSection.addOrUpdateItem(
                    model.get(AssistantPaymentRequestModel.PAYMENT_METHOD), true);
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.CONTACT_DETAILS) {
            view.mContactDetailsSection.addOrUpdateItem(
                    model.get(AssistantPaymentRequestModel.CONTACT_DETAILS), true);
            return true;
        } else if (propertyKey == AssistantPaymentRequestModel.TERMS_STATUS) {
            int termsStatus = model.get(AssistantPaymentRequestModel.TERMS_STATUS);
            view.mTermsSection.setTermsStatus(termsStatus);
            view.mTermsAsCheckboxSection.setTermsStatus(termsStatus);
            return true;
        }
        return false;
    }

    // TODO(crbug.com/806868): Move this logic to native.
    private void updateAvailableProfiles(AssistantPaymentRequestModel model, ViewHolder view) {
        List<PersonalDataManager.AutofillProfile> autofillProfiles =
                PersonalDataManager.getInstance().getProfilesToSuggest(
                        /* includeNameInLabel= */ false);

        // Suggest complete profiles first.
        Collections.sort(autofillProfiles, new CompletenessComparator(model));
        model.set(AssistantPaymentRequestModel.AVAILABLE_PROFILES, autofillProfiles);
    }

    /**
     * Updates the paddings between sections and section dividers.
     * @return whether the property key was handled.
     */
    private boolean updateSectionPaddings(
            AssistantPaymentRequestModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey != AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS)
                && (propertyKey != AssistantPaymentRequestModel.REQUEST_NAME)
                && (propertyKey != AssistantPaymentRequestModel.REQUEST_EMAIL)
                && (propertyKey != AssistantPaymentRequestModel.REQUEST_PHONE)
                && (propertyKey != AssistantPaymentRequestModel.REQUEST_PAYMENT)
                && (propertyKey != AssistantPaymentRequestModel.EXPANDED_SECTION)) {
            return false;
        }

        // Update section paddings such that the first and last section are flush to the top/bottom,
        // and all other sections have the same amount of padding in-between them.
        if (shouldShowContactDetails(model)) {
            view.mContactDetailsSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantPaymentRequestModel.REQUEST_PAYMENT)) {
            view.mPaymentMethodSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantPaymentRequestModel.REQUEST_SHIPPING_ADDRESS)) {
            view.mShippingAddressSection.setPaddings(0, view.mSectionToSectionPadding);
        }
        view.mTermsSection.setPaddings(view.mSectionToSectionPadding, 0);
        view.mTermsAsCheckboxSection.setPaddings(view.mSectionToSectionPadding, 0);

        // Hide dividers for currently invisible sections and after the expanded section, if any.
        boolean prevSectionIsExpandedOrInvisible = false;
        for (int i = 0; i < view.mPaymentRequestExpanderAccordion.getChildCount(); i++) {
            View child = view.mPaymentRequestExpanderAccordion.getChildAt(i);
            if (child instanceof AssistantVerticalExpander) {
                prevSectionIsExpandedOrInvisible = ((AssistantVerticalExpander) child).isExpanded()
                        || child.getVisibility() != View.VISIBLE;
            } else if (child.getTag() == view.mDividerTag) {
                child.setVisibility(prevSectionIsExpandedOrInvisible ? View.GONE : View.VISIBLE);
            }
        }
        return true;
    }

    /**
     * Updates/recreates section editors.
     * @return whether the property key was handled.
     */
    private boolean updateEditors(
            AssistantPaymentRequestModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey != AssistantPaymentRequestModel.WEB_CONTENTS)
                && (propertyKey != AssistantPaymentRequestModel.REQUEST_NAME)
                && (propertyKey != AssistantPaymentRequestModel.REQUEST_EMAIL)
                && (propertyKey != AssistantPaymentRequestModel.REQUEST_PHONE)
                && (propertyKey
                        != AssistantPaymentRequestModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS)) {
            return false;
        }

        WebContents webContents = model.get(AssistantPaymentRequestModel.WEB_CONTENTS);
        if (webContents == null) {
            view.mContactDetailsSection.setEditor(null);
            view.mPaymentMethodSection.setEditor(null);
            view.mShippingAddressSection.setEditor(null);
            return true;
        }

        if (shouldShowContactDetails(model)) {
            ContactEditor contactEditor =
                    new ContactEditor(model.get(AssistantPaymentRequestModel.REQUEST_NAME),
                            model.get(AssistantPaymentRequestModel.REQUEST_PHONE),
                            model.get(AssistantPaymentRequestModel.REQUEST_EMAIL),
                            !webContents.isIncognito());
            contactEditor.setEditorDialog(new EditorDialog(view.mActivity, null,
                    /*deleteRunnable =*/null));
            view.mContactDetailsSection.setEditor(contactEditor);
        }

        AddressEditor addressEditor = new AddressEditor(AddressEditor.Purpose.PAYMENT_REQUEST,
                /* saveToDisk= */ !webContents.isIncognito());
        addressEditor.setEditorDialog(new EditorDialog(view.mActivity, null,
                /*deleteRunnable =*/null));

        CardEditor cardEditor = new CardEditor(webContents, addressEditor,
                /* includeOrgLabel= */ false, /* observerForTest= */ null);
        Map<String, PaymentMethodData> paymentMethods =
                model.get(AssistantPaymentRequestModel.SUPPORTED_PAYMENT_METHODS);
        if (paymentMethods != null) {
            for (Map.Entry<String, PaymentMethodData> entry : paymentMethods.entrySet()) {
                cardEditor.addAcceptedPaymentMethodIfRecognized(entry.getValue());
            }
        }

        EditorDialog cardEditorDialog = new EditorDialog(view.mActivity, null,
                /*deleteRunnable =*/null);
        if (ChromeVersionInfo.isBetaBuild() || ChromeVersionInfo.isStableBuild()) {
            cardEditorDialog.disableScreenshots();
        }
        cardEditor.setEditorDialog(cardEditorDialog);

        view.mShippingAddressSection.setEditor(addressEditor);
        view.mPaymentMethodSection.setEditor(cardEditor);
        return true;
    }

    /**
     * Updates the map of supported payment methods (identifier -> methodData), filtered by
     * |SUPPORTED_BASIC_CARD_NETWORKS|.
     */
    // TODO(crbug.com/806868): Move the logic to retrieve and filter payment methods to native.
    private void updateAvailablePaymentMethods(AssistantPaymentRequestModel model) {
        // Only enable 'basic-card' payment method.
        PaymentMethodData methodData = new PaymentMethodData();
        methodData.supportedMethod = BasicCardUtils.BASIC_CARD_METHOD_NAME;

        // Apply basic-card filter if specified
        List<String> supportedBasicCardNetworks =
                model.get(AssistantPaymentRequestModel.SUPPORTED_BASIC_CARD_NETWORKS);
        if (supportedBasicCardNetworks != null && supportedBasicCardNetworks.size() > 0) {
            ArrayList<Integer> filteredNetworks = new ArrayList<>();
            Map<String, Integer> networks = BasicCardUtils.getNetworkIdentifiers();
            for (int i = 0; i < supportedBasicCardNetworks.size(); i++) {
                assert networks.containsKey(supportedBasicCardNetworks.get(i));
                filteredNetworks.add(networks.get(supportedBasicCardNetworks.get(i)));
            }

            methodData.supportedNetworks = new int[filteredNetworks.size()];
            for (int i = 0; i < filteredNetworks.size(); i++) {
                methodData.supportedNetworks[i] = filteredNetworks.get(i);
            }
        }

        Map<String, PaymentMethodData> supportedPaymentMethods = new HashMap<>();
        supportedPaymentMethods.put(BasicCardUtils.BASIC_CARD_METHOD_NAME, methodData);
        model.set(AssistantPaymentRequestModel.SUPPORTED_PAYMENT_METHODS, supportedPaymentMethods);
    }

    /**
     * Updates the list of available autofill payment methods (i.e., the list of available payment
     * methods available to the user as items in the UI).
     */
    // TODO(crbug.com/806868): Move this logic to native.
    private void updateAvailableAutofillPaymentMethods(AssistantPaymentRequestModel model) {
        WebContents webContents = model.get(AssistantPaymentRequestModel.WEB_CONTENTS);

        AutofillPaymentApp autofillPaymentApp = new AutofillPaymentApp(webContents);
        Map<String, PaymentMethodData> supportedPaymentMethods =
                model.get(AssistantPaymentRequestModel.SUPPORTED_PAYMENT_METHODS);
        List<PaymentInstrument> paymentMethods = autofillPaymentApp.getInstruments(
                supportedPaymentMethods != null ? supportedPaymentMethods : Collections.emptyMap(),
                /*forceReturnServerCards=*/true);

        List<AutofillPaymentInstrument> availablePaymentMethods = new ArrayList<>();
        for (PaymentInstrument method : paymentMethods) {
            if (method instanceof AutofillPaymentInstrument) {
                availablePaymentMethods.add(((AutofillPaymentInstrument) method));
            }
        }
        model.set(AssistantPaymentRequestModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS,
                availablePaymentMethods);
    }
}

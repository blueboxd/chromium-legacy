// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.v4.util.ArrayMap;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.NormalizedAddressRequestDelegate;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.page_info.CertificateChainHelper;
import org.chromium.chrome.browser.payments.ui.ContactDetailsSection;
import org.chromium.chrome.browser.payments.ui.LineItem;
import org.chromium.chrome.browser.payments.ui.PaymentInformation;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.FocusChangedObserver;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.browser.payments.ui.ShoppingCart;
import org.chromium.chrome.browser.preferences.MainPreferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ssl.SecurityStateModel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.prefeditor.Completable;
import org.chromium.chrome.browser.widget.prefeditor.EditableOption;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentDetailsConverter;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.components.payments.PaymentHandlerHost.PaymentHandlerHostDelegate;
import org.chromium.components.payments.PaymentValidator;
import org.chromium.components.payments.UrlUtil;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.AddressErrors;
import org.chromium.payments.mojom.CanMakePaymentQueryResult;
import org.chromium.payments.mojom.HasEnrolledInstrumentQueryResult;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PayerErrors;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentErrorReason;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentShippingType;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.net.URI;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Queue;
import java.util.Set;

/**
 * Android implementation of the PaymentRequest service defined in
 * third_party/blink/public/mojom/payments/payment_request.mojom.
 */
public class PaymentRequestImpl
        implements PaymentRequest, PaymentRequestUI.Client, PaymentApp.InstrumentsCallback,
                   PaymentApp.PaymentMethodChangeCallback, PaymentInstrument.AbortCallback,
                   PaymentInstrument.InstrumentDetailsCallback,
                   PaymentAppFactory.PaymentAppCreatedCallback,
                   PaymentResponseHelper.PaymentResponseRequesterDelegate, FocusChangedObserver,
                   NormalizedAddressRequestDelegate, SettingsAutofillAndPaymentsObserver.Observer,
                   PaymentHandlerHostDelegate, PaymentDetailsConverter.MethodChecker {
    /**
     * A delegate to ask questions about the system, that allows tests to inject behaviour without
     * having to modify the entire system. This partially mirrors a similar C++
     * (Content)PaymentRequestDelegate for the C++ implementation, allowing the test harness to
     * override behaviour in both in a similar fashion.
     */
    public interface Delegate {
        /**
         * Returns whether the ChromeActivity is currently showing an incognito tab.
         */
        boolean isIncognito(ChromeActivity activity);
        /**
         * Returns a non-null string if there is an invalid SSL certificate on the currently
         * loaded page.
         */
        String getInvalidSslCertificateErrorMessage(WebContents webContents);
        /**
         * Returns true if the given |webContents| is currently active. The |model| should be the
         * currently active TabModel from the activity.
         */
        boolean isWebContentsActive(TabModel model, WebContents webContents);
        /**
         * Returns whether the preferences allow CAN_MAKE_PAYMENT.
         */
        boolean prefsCanMakePayment();
        /**
         * Returns true if the UI can be skipped for "basic-card" scenarios. This will only ever
         * be true in tests.
         */
        boolean skipUiForBasicCard();
    }

    /**
     * A test-only observer for the PaymentRequest service implementation.
     */
    public interface PaymentRequestServiceObserverForTest {
        /**
         * Called after an instance of PaymentRequestImpl has been created.
         *
         * @param paymentRequest The newly created instance of PaymentRequestImpl.
         */
        void onPaymentRequestCreated(PaymentRequestImpl paymentRequest);

        /**
         * Called when an abort request was denied.
         */
        void onPaymentRequestServiceUnableToAbort();

        /**
         * Called when the controller is notified of billing address change, but does not alter the
         * editor UI.
         */
        void onPaymentRequestServiceBillingAddressChangeProcessed();

        /**
         * Called when the controller is notified of an expiration month change.
         */
        void onPaymentRequestServiceExpirationMonthChange();

        /**
         * Called when a show request failed. This can happen when:
         * <ul>
         *   <li>The merchant requests only unsupported payment methods.</li>
         *   <li>The merchant requests only payment methods that don't have instruments and are not
         *       able to add instruments from PaymentRequest UI.</li>
         * </ul>
         */
        void onPaymentRequestServiceShowFailed();

        /**
         * Called when the canMakePayment() request has been responded to.
         */
        void onPaymentRequestServiceCanMakePaymentQueryResponded();

        /**
         * Called when the hasEnrolledInstrument() request has been responded to.
         */
        void onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();

        /**
         * Called when the payment response is ready.
         */
        void onPaymentResponseReady();
    }

    /**
     * An observer interface injected when running tests to allow them to observe events.
     * This interface holds events that should be passed back to the native C++ test
     * harness and mirrors the C++ PaymentRequest::ObserverForTest() interface. Its methods
     * should be called in the same places that the C++ PaymentRequest object will call its
     * ObserverForTest.
     */
    public interface NativeObserverForTest {
        void onCanMakePaymentCalled();
        void onCanMakePaymentReturned();
        void onHasEnrolledInstrumentCalled();
        void onHasEnrolledInstrumentReturned();
        void onNotSupportedError();
        void onConnectionTerminated();
        void onAbortCalled();
    }

    /** Limit in the number of suggested items in a section. */
    public static final int SUGGESTIONS_LIMIT = 4;

    private static final String TAG = "cr_PaymentRequest";
    private static final String ANDROID_PAY_METHOD_NAME = "https://android.com/pay";
    private static final String PAY_WITH_GOOGLE_METHOD_NAME = "https://google.com/pay";
    // Reverse order of the comparator to sort in descending order of completeness scores.
    private static final Comparator<Completable> COMPLETENESS_COMPARATOR =
            (a, b) -> (compareCompletablesByCompleteness(b, a));

    /**
     * Sorts the payment instruments by several rules:
     * Rule 1: Non-autofill before autofill.
     * Rule 2: Complete instruments before incomplete intsruments.
     * Rule 3: Exact type matching instruments before non-exact type matching instruments.
     * Rule 4: Preselectable instruments before non-preselectable instruments.
     * Rule 5: Frequently and recently used instruments before rarely and non-recently used
     *         instruments.
     */
    private static final Comparator<PaymentInstrument> PAYMENT_INSTRUMENT_COMPARATOR = (a, b) -> {
        // Payment apps (not autofill) first.
        int autofill = (a.isAutofillInstrument() ? 1 : 0) - (b.isAutofillInstrument() ? 1 : 0);
        if (autofill != 0) return autofill;

        // Complete cards before cards with missing information.
        int completeness = compareCompletablesByCompleteness(b, a);
        if (completeness != 0) return completeness;

        // Cards with matching type before unknown type cards.
        int typeMatch = (b.isExactlyMatchingMerchantRequest() ? 1 : 0)
                - (a.isExactlyMatchingMerchantRequest() ? 1 : 0);
        if (typeMatch != 0) return typeMatch;

        // Preselectable instruments before non-preselectable instruments.
        // Note that this only affects service worker payment apps' instruments for now
        // since autofill payment instruments have already been sorted by preselect
        // after sorting by completeness and typeMatch. And the other payment apps'
        // instruments can always be preselected.
        int canPreselect = (b.canPreselect() ? 1 : 0) - (a.canPreselect() ? 1 : 0);
        if (canPreselect != 0) return canPreselect;

        // More frequently and recently used instruments first.
        return compareInstrumentsByFrecency(b, a);
    };

    private static PaymentRequestServiceObserverForTest sObserverForTest;
    private static boolean sIsLocalCanMakePaymentQueryQuotaEnforcedForTest;

    /**
     * True if show() was called in any PaymentRequestImpl object. Used to prevent showing more than
     * one PaymentRequest UI per browser process.
     */
    private static boolean sIsAnyPaymentRequestShowing;

    /** Monitors changes in the TabModelSelector. */
    private final TabModelSelectorObserver mSelectorObserver = new EmptyTabModelSelectorObserver() {
        @Override
        public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            disconnectFromClientWithDebugMessage(ErrorStrings.TAB_SWITCH);
        }
    };

    /** Monitors changes in the current TabModel. */
    private final TabModelObserver mTabModelObserver = new EmptyTabModelObserver() {
        @Override
        public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
            if (tab == null || tab.getId() != lastId) {
                mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
                disconnectFromClientWithDebugMessage(ErrorStrings.TAB_SWITCH);
            }
        }
    };

    /** Monitors changes in the tab overview. */
    private final OverviewModeObserver mOverviewModeObserver = new EmptyOverviewModeObserver() {
        @Override
        public void onOverviewModeStartedShowing(boolean showToolbar) {
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            disconnectFromClientWithDebugMessage(ErrorStrings.TAB_OVERVIEW_MODE);
        }
    };

    private final Handler mHandler = new Handler();
    private final RenderFrameHost mRenderFrameHost;
    private final Delegate mDelegate;
    private final NativeObserverForTest mNativeObserverForTest;
    private final WebContents mWebContents;
    private final String mTopLevelOrigin;
    private final String mPaymentRequestOrigin;
    private final String mMerchantName;
    @Nullable
    private final byte[][] mCertificateChain;
    private final AddressEditor mAddressEditor;
    private final CardEditor mCardEditor;
    private final JourneyLogger mJourneyLogger;
    private final boolean mIsIncognito;

    private PaymentRequestClient mClient;
    private boolean mIsCanMakePaymentResponsePending;
    private boolean mUseLegacyCanMakePayment;
    private boolean mIsHasEnrolledInstrumentResponsePending;
    private boolean mHasEnrolledInstrumentUsesPerMethodQuota;
    private boolean mIsCurrentPaymentRequestShowing;
    private boolean mWasRetryCalled;
    private final Queue<Runnable> mRetryQueue = new LinkedList<>();

    /**
     * The raw total amount being charged, as it was received from the website. This data is passed
     * to the payment app.
     */
    private PaymentItem mRawTotal;

    /**
     * The raw items in the shopping cart, as they were received from the website. This data is
     * passed to the payment app.
     */
    private List<PaymentItem> mRawLineItems;

    /**
     * A mapping from method names to modifiers, which include modified totals and additional line
     * items. Used to display modified totals for each payment instrument, modified total in order
     * summary, and additional line items in order summary.
     */
    private Map<String, PaymentDetailsModifier> mModifiers;

    /**
     * The UI model of the shopping cart, including the total. Each item includes a label and a
     * price string. This data is passed to the UI.
     */
    private ShoppingCart mUiShoppingCart;

    /**
     * The UI model for the shipping options. Includes the label and sublabel for each shipping
     * option. Also keeps track of the selected shipping option. This data is passed to the UI.
     */
    private SectionInformation mUiShippingOptions;

    private String mId;
    private Map<String, PaymentMethodData> mMethodData;
    private boolean mRequestShipping;
    private boolean mRequestPayerName;
    private boolean mRequestPayerPhone;
    private boolean mRequestPayerEmail;
    private int mShippingType;
    private SectionInformation mShippingAddressesSection;
    private ContactDetailsSection mContactSection;
    private List<PaymentApp> mApps;
    private List<PaymentApp> mPendingApps;
    private List<PaymentInstrument> mPendingInstruments;
    private int mPaymentMethodsSectionAdditionalTextResourceId;
    private SectionInformation mPaymentMethodsSection;
    private PaymentRequestUI mUI;
    private Callback<PaymentInformation> mPaymentInformationCallback;
    private PaymentInstrument mInvokedPaymentInstrument;
    private boolean mMerchantSupportsAutofillPaymentInstruments;
    private boolean mUserCanAddCreditCard;
    private boolean mHideServerAutofillInstruments;
    private ContactEditor mContactEditor;
    private boolean mHasRecordedAbortReason;
    private boolean mWaitForUpdatedDetails;
    private Map<String, CurrencyFormatter> mCurrencyFormatterMap;
    private TabModelSelector mObservedTabModelSelector;
    private TabModel mObservedTabModel;
    private OverviewModeBehavior mOverviewModeBehavior;
    private PaymentHandlerHost mPaymentHandlerHost;

    /** Aborts should only be recorded if the Payment Request was shown to the user. */
    private boolean mShouldRecordAbortReason;

    /**
     * There are a few situations were the Payment Request can appear, from a code perspective, to
     * be shown more than once. This boolean is used to make sure it is only logged once.
     */
    private boolean mDidRecordShowEvent;

    /** True if any of the requested payment methods are supported. */
    private boolean mArePaymentMethodsSupported;

    /**
     * True after at least one usable payment instrument has been found. Should be read only after
     * all payment apps have been queried.
     */
    private boolean mHasEnrolledInstrument;

    /**
     * True if we should skip showing PaymentRequest UI.
     *
     * <p>In cases where there is a single payment app and the merchant does not request shipping
     * or billing, we can skip showing UI as Payment Request UI is not benefiting the user at all.
     */
    private boolean mShouldSkipShowingPaymentRequestUi;

    /** Whether PaymentRequest.show() was invoked with a user gesture. */
    private boolean mIsUserGestureShow;

    /** The helper to create and fill the response to send to the merchant. */
    private PaymentResponseHelper mPaymentResponseHelper;

    /** If not empty, use this error message for rejecting PaymentRequest.show(). */
    private String mRejectShowErrorMessage;

    /**
     * True when Payment Request is invoked on a prohibited origin (e.g., blob:) or with an invalid
     * SSL certificate (e.g., self-signed).
     */
    private boolean mIsProhibitedOriginOrInvalidSsl;

    /**
     * Builds the PaymentRequest service implementation.
     *
     * @param renderFrameHost The host of the frame that has invoked the PaymentRequest API.
     */
    public PaymentRequestImpl(RenderFrameHost renderFrameHost, Delegate delegate,
            NativeObserverForTest nativeObserver) {
        assert renderFrameHost != null;

        mRenderFrameHost = renderFrameHost;
        mDelegate = delegate;
        mNativeObserverForTest = nativeObserver;
        mWebContents = WebContentsStatics.fromRenderFrameHost(renderFrameHost);

        mPaymentRequestOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(mRenderFrameHost.getLastCommittedURL());
        mTopLevelOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(mWebContents.getLastCommittedUrl());

        mMerchantName = mWebContents.getTitle();

        mCertificateChain = CertificateChainHelper.getCertificateChain(mWebContents);

        mApps = new ArrayList<>();

        mIsIncognito = mDelegate.isIncognito(ChromeActivity.fromWebContents(mWebContents));

        // Do not persist changes on disk in incognito mode.
        mAddressEditor = new AddressEditor(
                AddressEditor.Purpose.PAYMENT_REQUEST, /*saveToDisk=*/!mIsIncognito);
        // PaymentRequest card editor does not show the organization name in the dropdown with the
        // billing address labels.
        mCardEditor = new CardEditor(
                mWebContents, mAddressEditor, /*includeOrgLabel=*/false, sObserverForTest);

        mJourneyLogger = new JourneyLogger(mIsIncognito, mWebContents);
        mCurrencyFormatterMap = new HashMap<>();

        if (sObserverForTest != null) sObserverForTest.onPaymentRequestCreated(this);
    }

    /**
     * Called by the merchant website to initialize the payment request data.
     */
    @Override
    public void init(PaymentRequestClient client, PaymentMethodData[] methodData,
            PaymentDetails details, PaymentOptions options) {
        if (mClient != null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ATTEMPTED_INITIALIZATION_TWICE);
            return;
        }

        if (client == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_STATE);
            return;
        }

        mClient = client;
        mMethodData = new HashMap<>();

        if (!OriginSecurityChecker.isOriginSecure(mWebContents.getLastCommittedUrl())) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.NOT_IN_A_SECURE_ORIGIN);
            return;
        }

        mRequestShipping = options != null && options.requestShipping;
        mRequestPayerName = options != null && options.requestPayerName;
        mRequestPayerPhone = options != null && options.requestPayerPhone;
        mRequestPayerEmail = options != null && options.requestPayerEmail;
        mShippingType = options == null ? PaymentShippingType.SHIPPING : options.shippingType;

        // TODO(crbug.com/978471): Improve architecture for handling prohibited origins and invalid
        // SSL certificates.
        if (!UrlUtil.isOriginAllowedToUseWebPaymentApis(mWebContents.getLastCommittedUrl())) {
            mIsProhibitedOriginOrInvalidSsl = true;
            mRejectShowErrorMessage = ErrorStrings.PROHIBITED_ORIGIN;
            Log.d(TAG, mRejectShowErrorMessage);
            Log.d(TAG, ErrorStrings.PROHIBITED_ORIGIN_OR_INVALID_SSL_EXPLANATION);
            // Don't show any UI. Resolve .canMakePayment() with "false". Reject .show() with
            // "NotSupportedError".
            onAllPaymentAppsCreated();
            return;
        }

        mJourneyLogger.setRequestedInformation(
                mRequestShipping, mRequestPayerEmail, mRequestPayerPhone, mRequestPayerName);

        assert mRejectShowErrorMessage == null;
        mRejectShowErrorMessage = mDelegate.getInvalidSslCertificateErrorMessage(mWebContents);
        if (!TextUtils.isEmpty(mRejectShowErrorMessage)) {
            mIsProhibitedOriginOrInvalidSsl = true;
            Log.d(TAG, mRejectShowErrorMessage);
            Log.d(TAG, ErrorStrings.PROHIBITED_ORIGIN_OR_INVALID_SSL_EXPLANATION);
            // Don't show any UI. Resolve .canMakePayment() with "false". Reject .show() with
            // "NotSupportedError".
            onAllPaymentAppsCreated();
            return;
        }

        mMethodData = getValidatedMethodData(methodData, mCardEditor);
        if (mMethodData == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA);
            return;
        }

        if (!parseAndValidateDetailsOrDisconnectFromClient(details)) return;

        if (mRawTotal == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.TOTAL_REQUIRED);
            return;
        }
        mId = details.id;

        // Checks whether the merchant supports autofill payment instrument before show is called.
        mMerchantSupportsAutofillPaymentInstruments =
                AutofillPaymentApp.merchantSupportsAutofillPaymentInstruments(mMethodData);
        mUserCanAddCreditCard = mMerchantSupportsAutofillPaymentInstruments
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.NO_CREDIT_CARD_ABORT);

        boolean mayCrawl = !mUserCanAddCreditCard
                || PaymentsExperimentalFeatures.isEnabled(
                        ChromeFeatureList.WEB_PAYMENTS_ALWAYS_ALLOW_JUST_IN_TIME_PAYMENT_APP);
        PaymentAppFactory.getInstance().create(mWebContents,
                Collections.unmodifiableMap(mMethodData), mayCrawl, this /* callback */);

        // Log the various types of payment methods that were requested by the merchant.
        boolean requestedMethodGoogle = false;
        boolean requestedMethodOther = false;
        for (String methodName : mMethodData.keySet()) {
            if (methodName.equals(ANDROID_PAY_METHOD_NAME)
                    || methodName.equals(PAY_WITH_GOOGLE_METHOD_NAME)) {
                requestedMethodGoogle = true;
            } else if (methodName.startsWith(UrlConstants.HTTPS_URL_PREFIX)) {
                // Any method that starts with https and is not Android pay or Google pay is in the
                // "other" category.
                requestedMethodOther = true;
            }
        }
        mJourneyLogger.setRequestedPaymentMethodTypes(mMerchantSupportsAutofillPaymentInstruments,
                requestedMethodGoogle, requestedMethodOther);
        calculateWhetherShouldSkipShowingPaymentRequestUi(mDelegate.skipUiForBasicCard());
    }

    /**
     * Calculate whether the browser payment sheet should be skipped directly into the payment app.
     *
     * @param skipUiForNonUrlPaymentMethodIdentifiersForTest Whether non-URL payment method
     *                                                       identifiers should skip UI. Used only
     *                                                       in tests.
     */
    private void calculateWhetherShouldSkipShowingPaymentRequestUi(
            boolean skipUiForNonUrlPaymentMethodIdentifiersForTest) {
        // If there is a single payment method and the merchant has not requested any other
        // information, we can safely go directly to the payment app instead of showing
        // Payment Request UI.
        mShouldSkipShowingPaymentRequestUi =
                ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP)
                && mMethodData.size() == 1 && !mRequestShipping && !mRequestPayerName
                && !mRequestPayerPhone
                && !mRequestPayerEmail
                // Only allowing payment apps that own their own UIs.
                // This excludes AutofillPaymentApp as its UI is rendered inline in
                // the payment request UI, thus can't be skipped.
                && mMethodData.keySet().iterator().next() != null
                && (mMethodData.keySet().iterator().next().startsWith(UrlConstants.HTTPS_URL_PREFIX)
                        || skipUiForNonUrlPaymentMethodIdentifiersForTest);
    }

    /** @return Whether the UI was built. */
    private boolean buildUI(ChromeActivity activity) {
        assert activity != null;

        // Catch any time the user switches tabs. Because the dialog is modal, a user shouldn't be
        // allowed to switch tabs, which can happen if the user receives an external Intent.
        mObservedTabModelSelector = activity.getTabModelSelector();
        mObservedTabModel = activity.getCurrentTabModel();
        mObservedTabModelSelector.addObserver(mSelectorObserver);
        mObservedTabModel.addObserver(mTabModelObserver);

        // Only the currently selected tab is allowed to show the payment UI.
        if (!mDelegate.isWebContentsActive(mObservedTabModel, mWebContents)) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_SHOW_IN_BACKGROUND_TAB);
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
            return false;
        }

        // Catch any time the user enters the overview mode and dismiss the payment UI.
        if (activity instanceof ChromeTabbedActivity) {
            mOverviewModeBehavior = ((ChromeTabbedActivity) activity).getOverviewModeBehavior();
            if (mOverviewModeBehavior.overviewVisible()) {
                mJourneyLogger.setNotShown(NotShownReason.OTHER);
                disconnectFromClientWithDebugMessage(ErrorStrings.TAB_OVERVIEW_MODE);
                if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
                return false;
            }
            mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        }

        List<AutofillProfile> profiles = null;
        if (mRequestShipping || mRequestPayerName || mRequestPayerPhone || mRequestPayerEmail) {
            profiles = PersonalDataManager.getInstance().getProfilesToSuggest(
                    false /* includeNameInLabel */);
        }

        if (mRequestShipping && !mWaitForUpdatedDetails) {
            createShippingSection(activity, Collections.unmodifiableList(profiles));
        }

        if (mRequestPayerName || mRequestPayerPhone || mRequestPayerEmail) {
            // Do not persist changes on disk in incognito mode.
            mContactEditor = new ContactEditor(mRequestPayerName, mRequestPayerPhone,
                    mRequestPayerEmail, /*saveToDisk=*/!mIsIncognito);
            mContactSection = new ContactDetailsSection(activity,
                    Collections.unmodifiableList(profiles), mContactEditor, mJourneyLogger);
        }

        setIsAnyPaymentRequestShowing(true);
        mUI = new PaymentRequestUI(activity, this, mRequestShipping,
                /* requestShippingOption= */ mRequestShipping,
                mRequestPayerName || mRequestPayerPhone || mRequestPayerEmail,
                mMerchantSupportsAutofillPaymentInstruments,
                !PaymentPreferencesUtil.isPaymentCompleteOnce(), mMerchantName, mTopLevelOrigin,
                SecurityStateModel.getSecurityLevelForWebContents(mWebContents),
                new ShippingStrings(mShippingType));

        final FaviconHelper faviconHelper = new FaviconHelper();
        faviconHelper.getLocalFaviconImageForURL(Profile.getLastUsedProfile(),
                mWebContents.getLastCommittedUrl(),
                activity.getResources().getDimensionPixelSize(R.dimen.payments_favicon_size),
                (bitmap, iconUrl) -> {
                    if (mClient != null && bitmap == null) mClient.warnNoFavicon();
                    if (mUI != null && bitmap != null) mUI.setTitleBitmap(bitmap);
                    faviconHelper.destroy();
                });

        // Add the callback to change the label of shipping addresses depending on the focus.
        if (mRequestShipping) mUI.setShippingAddressSectionFocusChangedObserver(this);

        mAddressEditor.setEditorDialog(mUI.getEditorDialog());
        mCardEditor.setEditorDialog(mUI.getCardEditorDialog());
        if (mContactEditor != null) mContactEditor.setEditorDialog(mUI.getEditorDialog());

        return true;
    }

    private void createShippingSection(
            Context context, List<AutofillProfile> unmodifiableProfiles) {
        List<AutofillAddress> addresses = new ArrayList<>();

        for (int i = 0; i < unmodifiableProfiles.size(); i++) {
            AutofillProfile profile = unmodifiableProfiles.get(i);
            mAddressEditor.addPhoneNumberIfValid(profile.getPhoneNumber());

            // Only suggest addresses that have a street address.
            if (!TextUtils.isEmpty(profile.getStreetAddress())) {
                addresses.add(new AutofillAddress(context, profile));
            }
        }

        // Suggest complete addresses first.
        Collections.sort(addresses, COMPLETENESS_COMPARATOR);

        // Limit the number of suggestions.
        addresses = addresses.subList(0, Math.min(addresses.size(), SUGGESTIONS_LIMIT));

        // Load the validation rules for each unique region code.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < addresses.size(); ++i) {
            String countryCode = AutofillAddress.getCountryCode(addresses.get(i).getProfile());
            if (!uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                PersonalDataManager.getInstance().loadRulesForAddressNormalization(countryCode);
            }
        }

        // Automatically select the first address if one is complete and if the merchant does
        // not require a shipping address to calculate shipping costs.
        boolean hasCompleteShippingAddress = !addresses.isEmpty() && addresses.get(0).isComplete();
        int firstCompleteAddressIndex = SectionInformation.NO_SELECTION;
        if (mUiShippingOptions.getSelectedItem() != null && hasCompleteShippingAddress) {
            firstCompleteAddressIndex = 0;

            // The initial label for the selected shipping address should not include the
            // country.
            addresses.get(firstCompleteAddressIndex).setShippingAddressLabelWithoutCountry();
        }

        // Log the number of suggested shipping addresses and whether at least one of them is
        // complete.
        mJourneyLogger.setNumberOfSuggestionsShown(
                Section.SHIPPING_ADDRESS, addresses.size(), hasCompleteShippingAddress);

        int missingFields = 0;
        if (addresses.isEmpty()) {
            // All fields are missing.
            missingFields = AutofillAddress.CompletionStatus.INVALID_RECIPIENT
                    | AutofillAddress.CompletionStatus.INVALID_PHONE_NUMBER
                    | AutofillAddress.CompletionStatus.INVALID_ADDRESS;
        } else {
            missingFields = addresses.get(0).getMissingFieldsOfShippingProfile();
        }
        if (missingFields != 0) {
            RecordHistogram.recordSparseHistogram(
                    "PaymentRequest.MissingShippingFields", missingFields);
        }

        mShippingAddressesSection = new SectionInformation(
                PaymentRequestUI.DataType.SHIPPING_ADDRESSES, firstCompleteAddressIndex, addresses);
    }

    /**
     * Called by the merchant website to show the payment request to the user.
     */
    @Override
    public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        if (mClient == null) return;

        if (mUI != null) {
            // Can be triggered only by a compromised renderer. In normal operation, calling show()
            // twice on the same instance of PaymentRequest in JavaScript is rejected at the
            // renderer level.
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_SHOW_TWICE);
            return;
        }

        if (getIsAnyPaymentRequestShowing()) {
            // The renderer can create multiple instances of PaymentRequest and call show() on each
            // one. Only the first one will be shown. This also prevents multiple tabs and windows
            // from showing PaymentRequest UI at the same time.
            mJourneyLogger.setNotShown(NotShownReason.CONCURRENT_REQUESTS);
            disconnectFromClientWithDebugMessage(
                    ErrorStrings.ANOTHER_UI_SHOWING, PaymentErrorReason.ALREADY_SHOWING);
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
            return;
        }

        mIsCurrentPaymentRequestShowing = true;
        if (disconnectIfNoPaymentMethodsSupported()) return;

        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
            return;
        }

        mIsUserGestureShow = isUserGesture;
        mWaitForUpdatedDetails = waitForUpdatedDetails;

        if (!mShouldSkipShowingPaymentRequestUi) {
            if (!buildUI(chromeActivity)) return;
            mUI.show();
        }

        triggerPaymentAppUiSkipIfApplicable(chromeActivity);
    }

    private void triggerPaymentAppUiSkipIfApplicable(ChromeActivity chromeActivity) {
        // If we are skipping showing the Payment Request UI, we should call into the
        // PaymentApp immediately after we determine the instruments are ready and UI is shown.
        if (mShouldSkipShowingPaymentRequestUi && isFinishedQueryingPaymentApps()
                && mIsCurrentPaymentRequestShowing && !mWaitForUpdatedDetails) {
            assert !mPaymentMethodsSection.isEmpty();

            PaymentInstrument selectedInstrument =
                    (PaymentInstrument) mPaymentMethodsSection.getSelectedItem();
            if (!buildUI(chromeActivity)) return;

            // Do not skip to payment app if it is not the only one, it's not pre-selected, or if
            // skip-UI requires a user gesture in show(), which was not present.
            // Note that ServiceWorkerPaymentApp cannot be pre-selected if its name and/or icon is
            // missing.
            if (mPaymentMethodsSection.getSize() > 1 || selectedInstrument == null
                    || (selectedInstrument.isUserGestureRequiredToSkipUi()
                            && !mIsUserGestureShow)) {
                mUI.show();
            } else {
                mUI.dimBackground();
                mDidRecordShowEvent = true;
                mShouldRecordAbortReason = true;
                mJourneyLogger.setEventOccurred(Event.SKIPPED_SHOW);
                assert mRawTotal != null;
                // The total amount in details should be finalized at this point. So it is safe to
                // record the triggered transaction amount.
                assert !mWaitForUpdatedDetails;
                mJourneyLogger.recordTransactionAmount(
                        mRawTotal.amount.currency, mRawTotal.amount.value, false /*completed*/);

                onPayClicked(null /* selectedShippingAddress */, null /* selectedShippingOption */,
                        selectedInstrument);
            }
        }
    }

    private static Map<String, PaymentMethodData> getValidatedMethodData(
            PaymentMethodData[] methodData, CardEditor paymentMethodsCollector) {
        // Payment methodData are required.
        if (methodData == null || methodData.length == 0) return null;
        Map<String, PaymentMethodData> result = new ArrayMap<>();
        for (int i = 0; i < methodData.length; i++) {
            String method = methodData[i].supportedMethod;

            if (TextUtils.isEmpty(method)) return null;
            result.put(method, methodData[i]);

            paymentMethodsCollector.addAcceptedPaymentMethodIfRecognized(methodData[i]);
        }

        return Collections.unmodifiableMap(result);
    }

    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        mApps.add(paymentApp);
    }

    @Override
    public void onGetPaymentAppsError(String errorMessage) {
        if (TextUtils.isEmpty(mRejectShowErrorMessage)) mRejectShowErrorMessage = errorMessage;
    }

    @Override
    public void onAllPaymentAppsCreated() {
        if (mClient == null) return;

        assert mPendingApps == null;

        dedupePaymentApps();

        mPendingApps = new ArrayList<>(mApps);
        mPendingInstruments = new ArrayList<>();

        Map<PaymentApp, Map<String, PaymentMethodData>> queryApps = new ArrayMap<>();
        for (int i = 0; i < mApps.size(); i++) {
            PaymentApp app = mApps.get(i);
            Map<String, PaymentMethodData> appMethods =
                    filterMerchantMethodData(mMethodData, app.getAppMethodNames());
            if (appMethods == null || !app.supportsMethodsAndData(appMethods)) {
                mPendingApps.remove(app);
            } else {
                mArePaymentMethodsSupported = true;
                queryApps.put(app, appMethods);
            }
        }

        if (mIsCanMakePaymentResponsePending) {
            // New canMakePayment doesn't need to wait for all apps to be queried.
            if (!mUseLegacyCanMakePayment || queryApps.isEmpty()) {
                respondCanMakePaymentQuery(mUseLegacyCanMakePayment);
            }
        }

        if (mIsHasEnrolledInstrumentResponsePending && queryApps.isEmpty()) {
            respondHasEnrolledInstrumentQuery(mHasEnrolledInstrument);
        }

        if (disconnectIfNoPaymentMethodsSupported()) return;

        for (Map.Entry<PaymentApp, Map<String, PaymentMethodData>> q : queryApps.entrySet()) {
            q.getKey().setPaymentMethodChangeCallback(this);
            q.getKey().getInstruments(mId, q.getValue(), mTopLevelOrigin, mPaymentRequestOrigin,
                    mCertificateChain,
                    mModifiers == null ? new HashMap<>() : Collections.unmodifiableMap(mModifiers),
                    this);
        }
    }

    // Dedupe payment apps according to preferred related applications and can deduped application.
    // Note that this is only work for deduping service worker based payment app from native Android
    // payment app for now. The identifier of a native Android payment app is its package name. The
    // identifier of a service worker based payment app is its registration scope which equals to
    // corresponding native android payment app's default method name.
    private void dedupePaymentApps() {
        // Dedupe ServiceWorkerPaymentApp according to preferred related applications from
        // ServiceWorkerPaymentApps.
        Set<String> appIdentifiers = new HashSet<>();
        for (int i = 0; i < mApps.size(); i++) {
            appIdentifiers.add(mApps.get(i).getAppIdentifier());
        }
        List<PaymentApp> appsToDedupe = new ArrayList<>();
        for (int i = 0; i < mApps.size(); i++) {
            Set<String> applicationIds = mApps.get(i).getPreferredRelatedApplicationIds();
            if (applicationIds == null || applicationIds.isEmpty()) continue;
            for (String id : applicationIds) {
                if (appIdentifiers.contains(id)) {
                    appsToDedupe.add(mApps.get(i));
                    break;
                }
            }
        }
        if (!appsToDedupe.isEmpty()) mApps.removeAll(appsToDedupe);

        // Dedupe ServiceWorkerPaymentApp according to can deduped applications from native android
        // payment apps.
        Set<String> canDedupedApplicationIds = new HashSet<>();
        for (int i = 0; i < mApps.size(); i++) {
            URI canDedupedApplicationIdUri = mApps.get(i).getCanDedupedApplicationId();
            if (canDedupedApplicationIdUri == null) continue;
            String canDedupedApplicationId = canDedupedApplicationIdUri.toString();
            if (TextUtils.isEmpty(canDedupedApplicationId)) continue;
            canDedupedApplicationIds.add(canDedupedApplicationId);
            // Add the trailing slash, because Service worker registration scope is a directory path
            // that must end with a '/' (e.g., "https://google.com/pay/"), whereas
            // "canDedupedApplicationIdUri" is derived from the native Android payment app's default
            // URL-based payment method name that may not necessarily specify the trailing slash
            // (e.g., "https://google.com/pay").
            if (canDedupedApplicationId.charAt(canDedupedApplicationId.length() - 1) != '/') {
                canDedupedApplicationIds.add(canDedupedApplicationId + '/');
            }
        }
        for (String appId : canDedupedApplicationIds) {
            for (int i = 0; i < mApps.size(); i++) {
                if (appId.equals(mApps.get(i).getAppIdentifier())) {
                    mApps.remove(i);
                    break;
                }
            }
        }
    }

    /** Filter out merchant method data that's not relevant to a payment app. Can return null. */
    private static Map<String, PaymentMethodData> filterMerchantMethodData(
            Map<String, PaymentMethodData> merchantMethodData, Set<String> appMethods) {
        Map<String, PaymentMethodData> result = null;
        for (String method : appMethods) {
            if (merchantMethodData.containsKey(method)) {
                if (result == null) result = new ArrayMap<>();
                result.put(method, merchantMethodData.get(method));
            }
        }
        return result == null ? null : Collections.unmodifiableMap(result);
    }

    /** Called by the payment app to get updated total based on the billing address, for example. */
    @Override
    public boolean changePaymentMethodFromInvokedApp(String methodName, String stringifiedDetails) {
        if (TextUtils.isEmpty(methodName) || stringifiedDetails == null || mClient == null
                || mInvokedPaymentInstrument == null) {
            return false;
        }

        mClient.onPaymentMethodChange(methodName, stringifiedDetails);
        return true;
    }

    /**
     * Called by the web-based payment handler to get updated total based on the billing address,
     * for example.
     */
    @Override
    public boolean changePaymentMethodFromPaymentHandler(
            String methodName, String stringifiedData) {
        if (mPaymentHandlerHost == null || mPaymentHandlerHost.isChangingPaymentMethod()) {
            return false;
        }

        return changePaymentMethodFromInvokedApp(methodName, stringifiedData);
    }

    @Override
    public boolean isInvokedInstrumentValidForPaymentMethodIdentifier(String methodName) {
        return mInvokedPaymentInstrument != null
                && mInvokedPaymentInstrument.isValidForPaymentMethodData(methodName, null);
    }

    /**
     * Called by merchant to update the shipping options and line items after the user has selected
     * their shipping address or shipping option.
     */
    @Override
    public void updateWith(PaymentDetails details) {
        if (mClient == null) return;

        if (mWaitForUpdatedDetails) {
            initializeWithUpdatedDetails(details);
            return;
        }

        if (mUI == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW);
            return;
        }

        if (!mRequestShipping && !mRequestPayerName && !mRequestPayerEmail && !mRequestPayerPhone
                && (mInvokedPaymentInstrument == null
                        || !mInvokedPaymentInstrument.isChangingPaymentMethod())) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_STATE);
            return;
        }

        if (!parseAndValidateDetailsOrDisconnectFromClient(details)) return;

        if (mInvokedPaymentInstrument != null
                && mInvokedPaymentInstrument.isChangingPaymentMethod()) {
            // After a payment app has been invoked, all of the merchant's calls to update the price
            // via updateWith() should be forwarded to the invoked app, so it can reflect the
            // updated price in its UI.
            //
            // For historical reasons, a PaymentApp is what genertes PaymentInstrument objects that
            // Chrome can invoke. However, a PaymentInstrument can represent the whole app with
            // opaque to Chrome sub-instruments inside, representing each card in the user account.
            // Hence Chrome forwards the updateWith() calls to the currently invoked
            // PaymentInstrument object.
            mInvokedPaymentInstrument.updateWith(
                    PaymentDetailsConverter.convertToPaymentMethodChangeResponse(
                            details, this /* methodChecker */));
            return;
        }

        if (mRequestShipping && (mUiShippingOptions.isEmpty() || !TextUtils.isEmpty(details.error))
                && mShippingAddressesSection.getSelectedItem() != null) {
            mShippingAddressesSection.getSelectedItem().setInvalid();
            mShippingAddressesSection.setSelectedItemIndex(SectionInformation.INVALID_SELECTION);
            mShippingAddressesSection.setErrorMessage(details.error);
        }

        enableUserInterfaceAfterPaymentRequestUpdateEvent();
    }

    private void initializeWithUpdatedDetails(PaymentDetails details) {
        assert mWaitForUpdatedDetails;

        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        if (!parseAndValidateDetailsOrDisconnectFromClient(details)) return;

        if (!TextUtils.isEmpty(details.error)) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_STATE);
            return;
        }

        if (mRequestShipping) {
            createShippingSection(chromeActivity,
                    Collections.unmodifiableList(
                            PersonalDataManager.getInstance().getProfilesToSuggest(
                                    false /* includeNameInLabel */)));
        }

        if (!mShouldSkipShowingPaymentRequestUi) {
            enableUserInterfaceAfterPaymentRequestUpdateEvent();
        }

        mWaitForUpdatedDetails = false;
        // Triggered tansaction amount gets recorded when both of the following conditions are met:
        // 1- Either Event.Shown or Event.SKIPPED_SHOW bits are set showing that transaction is
        // triggered (mDidRecordShowEvent == true). 2- The total amount in details won't change
        // (mWaitForUpdatedDetails == false). Record the transaction amount only when the triggered
        // condition is already met. Otherwise it will get recorded when triggered condition becomes
        // true.
        if (mDidRecordShowEvent) {
            assert mRawTotal != null;
            mJourneyLogger.recordTransactionAmount(
                    mRawTotal.amount.currency, mRawTotal.amount.value, false /*completed*/);
        }

        triggerPaymentAppUiSkipIfApplicable(chromeActivity);
    }

    /**
     * Called when the merchant received a new shipping address, shipping option, or payment method
     * info, but did not update the payment details in response.
     */
    @Override
    public void noUpdatedPaymentDetails() {
        if (mClient == null) return;

        if (mUI == null) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.CANNOT_UPDATE_WITHOUT_SHOW);
            return;
        }

        if (mInvokedPaymentInstrument != null
                && mInvokedPaymentInstrument.isChangingPaymentMethod()) {
            mInvokedPaymentInstrument.noUpdatedPaymentDetails();
            return;
        }

        enableUserInterfaceAfterPaymentRequestUpdateEvent();
    }

    private void enableUserInterfaceAfterPaymentRequestUpdateEvent() {
        if (mPaymentInformationCallback != null && mPaymentMethodsSection != null) {
            providePaymentInformation();
        } else {
            mUI.updateOrderSummarySection(mUiShoppingCart);
            if (mRequestShipping) {
                mUI.updateSection(PaymentRequestUI.DataType.SHIPPING_OPTIONS, mUiShippingOptions);
            }
        }
    }

    /**
     * Sets the total, display line items, and shipping options based on input and returns the
     * status boolean. That status is true for valid data, false for invalid data. If the input is
     * invalid, disconnects from the client. Both raw and UI versions of data are updated.
     *
     * @param details The total, line items, and shipping options to parse, validate, and save in
     *                member variables.
     * @return True if the data is valid. False if the data is invalid.
     */
    private boolean parseAndValidateDetailsOrDisconnectFromClient(PaymentDetails details) {
        if (!PaymentValidator.validatePaymentDetails(details)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return false;
        }

        if (details.total != null) {
            mRawTotal = details.total;
        }

        if (mRawLineItems == null || details.displayItems != null) {
            mRawLineItems = Collections.unmodifiableList(details.displayItems != null
                            ? Arrays.asList(details.displayItems)
                            : new ArrayList<>());
        }

        loadCurrencyFormattersForPaymentDetails(details);

        // Total is never pending.
        CurrencyFormatter formatter = getOrCreateCurrencyFormatter(mRawTotal.amount);
        LineItem uiTotal = new LineItem(mRawTotal.label, formatter.getFormattedCurrencyCode(),
                formatter.format(mRawTotal.amount.value), /* isPending */ false);

        List<LineItem> uiLineItems = getLineItems(mRawLineItems);

        mUiShoppingCart = new ShoppingCart(uiTotal, uiLineItems);

        if (mUiShippingOptions == null || details.shippingOptions != null) {
            mUiShippingOptions = getShippingOptions(details.shippingOptions);
        }

        if (details.modifiers != null) {
            if (details.modifiers.length == 0) mModifiers.clear();

            for (int i = 0; i < details.modifiers.length; i++) {
                PaymentDetailsModifier modifier = details.modifiers[i];
                String method = modifier.methodData.supportedMethod;
                if (mModifiers == null) mModifiers = new ArrayMap<>();
                mModifiers.put(method, modifier);
            }
        }

        updateInstrumentModifiedTotals();

        assert mRawTotal != null;
        assert mRawLineItems != null;

        return true;
    }

    /** Updates the modifiers for payment instruments and order summary. */
    private void updateInstrumentModifiedTotals() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS_MODIFIERS)) return;
        if (mModifiers == null) return;
        if (mPaymentMethodsSection == null) return;

        for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
            PaymentInstrument instrument = (PaymentInstrument) mPaymentMethodsSection.getItem(i);
            PaymentDetailsModifier modifier = getModifier(instrument);
            instrument.setModifiedTotal(modifier == null || modifier.total == null
                            ? null
                            : getOrCreateCurrencyFormatter(modifier.total.amount)
                                      .format(modifier.total.amount.value));
        }

        updateOrderSummary((PaymentInstrument) mPaymentMethodsSection.getSelectedItem());
    }

    /** Sets the modifier for the order summary based on the given instrument, if any. */
    private void updateOrderSummary(@Nullable PaymentInstrument instrument) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS_MODIFIERS)) return;

        PaymentDetailsModifier modifier = getModifier(instrument);
        PaymentItem total = modifier == null ? null : modifier.total;
        if (total == null) total = mRawTotal;

        CurrencyFormatter formatter = getOrCreateCurrencyFormatter(total.amount);
        mUiShoppingCart.setTotal(new LineItem(total.label, formatter.getFormattedCurrencyCode(),
                formatter.format(total.amount.value), false /* isPending */));
        mUiShoppingCart.setAdditionalContents(modifier == null
                        ? null
                        : getLineItems(Arrays.asList(modifier.additionalDisplayItems)));
        if (mUI != null) mUI.updateOrderSummarySection(mUiShoppingCart);
    }

    /** @return The first modifier that matches the given instrument, or null. */
    @Nullable
    private PaymentDetailsModifier getModifier(@Nullable PaymentInstrument instrument) {
        if (mModifiers == null || instrument == null) return null;
        // Makes a copy to ensure it is modifiable.
        Set<String> methodNames = new HashSet<>(instrument.getInstrumentMethodNames());
        methodNames.retainAll(mModifiers.keySet());
        if (methodNames.isEmpty()) return null;

        for (String methodName : methodNames) {
            PaymentDetailsModifier modifier = mModifiers.get(methodName);
            if (instrument.isValidForPaymentMethodData(methodName, modifier.methodData)) {
                return modifier;
            }
        }

        return null;
    }

    /**
     * Converts a list of payment items and returns their parsed representation.
     *
     * @param items The payment items to parse. Can be null.
     * @return A list of valid line items.
     */
    private List<LineItem> getLineItems(@Nullable List<PaymentItem> items) {
        // Line items are optional.
        if (items == null) return new ArrayList<>();

        List<LineItem> result = new ArrayList<>(items.size());
        for (int i = 0; i < items.size(); i++) {
            PaymentItem item = items.get(i);
            CurrencyFormatter formatter = getOrCreateCurrencyFormatter(item.amount);
            result.add(new LineItem(item.label,
                    isMixedOrChangedCurrency() ? formatter.getFormattedCurrencyCode() : "",
                    formatter.format(item.amount.value), item.pending));
        }

        return Collections.unmodifiableList(result);
    }

    /**
     * Converts a list of shipping options and returns their parsed representation.
     *
     * @param options The raw shipping options to parse. Can be null.
     * @return The UI representation of the shipping options.
     */
    private SectionInformation getShippingOptions(@Nullable PaymentShippingOption[] options) {
        // Shipping options are optional.
        if (options == null || options.length == 0) {
            return new SectionInformation(PaymentRequestUI.DataType.SHIPPING_OPTIONS);
        }

        List<EditableOption> result = new ArrayList<>();
        int selectedItemIndex = SectionInformation.NO_SELECTION;
        for (int i = 0; i < options.length; i++) {
            PaymentShippingOption option = options[i];
            CurrencyFormatter formatter = getOrCreateCurrencyFormatter(option.amount);
            String currencyPrefix = isMixedOrChangedCurrency()
                    ? formatter.getFormattedCurrencyCode() + "\u0020"
                    : "";
            result.add(new EditableOption(option.id, option.label,
                    currencyPrefix + formatter.format(option.amount.value), null));
            if (option.selected) selectedItemIndex = i;
        }

        return new SectionInformation(PaymentRequestUI.DataType.SHIPPING_OPTIONS, selectedItemIndex,
                Collections.unmodifiableList(result));
    }

    /**
     * Load required currency formatter for a given PaymentDetails.
     *
     * Note that the cache (mCurrencyFormatterMap) is not cleared for
     * updated payment details so as to indicate the currency has been changed.
     *
     * @param details The given payment details.
     */
    private void loadCurrencyFormattersForPaymentDetails(PaymentDetails details) {
        if (details.total != null) {
            getOrCreateCurrencyFormatter(details.total.amount);
        }

        if (details.displayItems != null) {
            for (PaymentItem item : details.displayItems) {
                getOrCreateCurrencyFormatter(item.amount);
            }
        }

        if (details.shippingOptions != null) {
            for (PaymentShippingOption option : details.shippingOptions) {
                getOrCreateCurrencyFormatter(option.amount);
            }
        }

        if (details.modifiers != null) {
            for (PaymentDetailsModifier modifier : details.modifiers) {
                if (modifier.total != null) getOrCreateCurrencyFormatter(modifier.total.amount);
                for (PaymentItem displayItem : modifier.additionalDisplayItems) {
                    getOrCreateCurrencyFormatter(displayItem.amount);
                }
            }
        }
    }

    private boolean isMixedOrChangedCurrency() {
        return mCurrencyFormatterMap.size() > 1;
    }

    /**
     * Gets currency formatter for a given PaymentCurrencyAmount,
     * creates one if no existing instance is found.
     *
     * @amount The given payment amount.
     */
    private CurrencyFormatter getOrCreateCurrencyFormatter(PaymentCurrencyAmount amount) {
        String key = amount.currency;
        CurrencyFormatter formatter = mCurrencyFormatterMap.get(key);
        if (formatter == null) {
            formatter = new CurrencyFormatter(amount.currency, Locale.getDefault());
            mCurrencyFormatterMap.put(key, formatter);
        }
        return formatter;
    }

    /**
     * Called to retrieve the data to show in the initial PaymentRequest UI.
     */
    @Override
    public void getDefaultPaymentInformation(Callback<PaymentInformation> callback) {
        mPaymentInformationCallback = callback;

        if (mPaymentMethodsSection == null || mWaitForUpdatedDetails) return;

        mHandler.post(() -> {
            if (mUI != null) providePaymentInformation();
        });
    }

    private void providePaymentInformation() {
        // Do not display service worker payment instrument summary in single line so as to display
        // its origin completely.
        mPaymentMethodsSection.setDisplaySelectedItemSummaryInSingleLineInNormalMode(
                !(mPaymentMethodsSection.getSelectedItem() instanceof ServiceWorkerPaymentApp));
        mPaymentInformationCallback.onResult(
                new PaymentInformation(mUiShoppingCart, mShippingAddressesSection,
                        mUiShippingOptions, mContactSection, mPaymentMethodsSection));
        mPaymentInformationCallback = null;

        if (!mDidRecordShowEvent) {
            mDidRecordShowEvent = true;
            mShouldRecordAbortReason = true;
            mJourneyLogger.setEventOccurred(Event.SHOWN);
            // Record the triggered transaction amount only when the total amount in details is
            // finalized (i.e. mWaitForUpdatedDetails == false). Otherwise it will get recorded when
            // the updated details become available.
            if (!mWaitForUpdatedDetails) {
                assert mRawTotal != null;
                mJourneyLogger.recordTransactionAmount(
                        mRawTotal.amount.currency, mRawTotal.amount.value, false /*completed*/);
            }
        }
    }

    @Override
    public void getShoppingCart(final Callback<ShoppingCart> callback) {
        mHandler.post(() -> callback.onResult(mUiShoppingCart));
    }

    @Override
    public void getSectionInformation(@PaymentRequestUI.DataType final int optionType,
            final Callback<SectionInformation> callback) {
        mHandler.post(() -> {
            if (optionType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
                callback.onResult(mShippingAddressesSection);
            } else if (optionType == PaymentRequestUI.DataType.SHIPPING_OPTIONS) {
                callback.onResult(mUiShippingOptions);
            } else if (optionType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
                callback.onResult(mContactSection);
            } else if (optionType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
                assert mPaymentMethodsSection != null;
                callback.onResult(mPaymentMethodsSection);
            }
        });
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionOptionSelected(@PaymentRequestUI.DataType int optionType,
            EditableOption option, Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
            // Log the change of shipping address.
            mJourneyLogger.incrementSelectionChanges(Section.SHIPPING_ADDRESS);
            AutofillAddress address = (AutofillAddress) option;
            if (address.isComplete()) {
                mShippingAddressesSection.setSelectedItem(option);
                startShippingAddressChangeNormalization(address);
            } else {
                editAddress(address);
            }
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.SHIPPING_OPTIONS) {
            // This may update the line items.
            mUiShippingOptions.setSelectedItem(option);
            mClient.onShippingOptionChange(option.getIdentifier());
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
            // Log the change of contact info.
            mJourneyLogger.incrementSelectionChanges(Section.CONTACT_INFO);
            AutofillContact contact = (AutofillContact) option;
            if (contact.isComplete()) {
                mContactSection.setSelectedItem(option);
                if (!mWasRetryCalled) return PaymentRequestUI.SelectionResult.NONE;
                dispatchPayerDetailChangeEventIfNeeded(contact.toPayerDetail());
            } else {
                editContact(contact);
                if (!mWasRetryCalled) return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
            }
            mPaymentInformationCallback = callback;
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
            PaymentInstrument paymentInstrument = (PaymentInstrument) option;
            if (paymentInstrument instanceof AutofillPaymentInstrument) {
                AutofillPaymentInstrument card = (AutofillPaymentInstrument) paymentInstrument;

                if (!card.isComplete()) {
                    editCard(card);
                    return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
                }
            }
            // Log the change of payment method.
            mJourneyLogger.incrementSelectionChanges(Section.PAYMENT_METHOD);

            updateOrderSummary(paymentInstrument);
            mPaymentMethodsSection.setSelectedItem(option);
        }

        return PaymentRequestUI.SelectionResult.NONE;
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionEditOption(@PaymentRequestUI.DataType int optionType, EditableOption option,
            Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
            editAddress((AutofillAddress) option);
            mPaymentInformationCallback = callback;

            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        }

        if (optionType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
            editContact((AutofillContact) option);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        }

        if (optionType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
            editCard((AutofillPaymentInstrument) option);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        }

        assert false;
        return PaymentRequestUI.SelectionResult.NONE;
    }

    @Override
    @PaymentRequestUI.SelectionResult
    public int onSectionAddOption(
            @PaymentRequestUI.DataType int optionType, Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES) {
            editAddress(null);
            mPaymentInformationCallback = callback;
            // Log the add of shipping address.
            mJourneyLogger.incrementSelectionAdds(Section.SHIPPING_ADDRESS);
            return PaymentRequestUI.SelectionResult.ASYNCHRONOUS_VALIDATION;
        } else if (optionType == PaymentRequestUI.DataType.CONTACT_DETAILS) {
            editContact(null);
            // Log the add of contact info.
            mJourneyLogger.incrementSelectionAdds(Section.CONTACT_INFO);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        } else if (optionType == PaymentRequestUI.DataType.PAYMENT_METHODS) {
            editCard(null);
            // Log the add of credit card.
            mJourneyLogger.incrementSelectionAdds(Section.PAYMENT_METHOD);
            return PaymentRequestUI.SelectionResult.EDITOR_LAUNCH;
        }

        return PaymentRequestUI.SelectionResult.NONE;
    }

    private void editAddress(final AutofillAddress toEdit) {
        if (toEdit != null) {
            // Log the edit of a shipping address.
            mJourneyLogger.incrementSelectionEdits(Section.SHIPPING_ADDRESS);
        }
        mAddressEditor.edit(toEdit, new Callback<AutofillAddress>() {
            @Override
            public void onResult(AutofillAddress editedAddress) {
                if (mUI == null) return;

                if (editedAddress != null) {
                    mAddressEditor.setAddressErrors(null);

                    // Sets or updates the shipping address label.
                    editedAddress.setShippingAddressLabelWithCountry();

                    mCardEditor.updateBillingAddressIfComplete(editedAddress);

                    // A partial or complete address came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedAddress.isComplete()) {
                        // If the address is not complete, unselect it (editor can return incomplete
                        // information when cancelled).
                        mShippingAddressesSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                        providePaymentInformation();
                    } else {
                        if (toEdit == null) {
                            // Address is complete and user was in the "Add flow": add an item to
                            // the list.
                            mShippingAddressesSection.addAndSelectItem(editedAddress);
                        }

                        if (mContactSection != null) {
                            // Update |mContactSection| with the new/edited address, which will
                            // update an existing item or add a new one to the end of the list.
                            mContactSection.addOrUpdateWithAutofillAddress(editedAddress);
                            mUI.updateSection(
                                    PaymentRequestUI.DataType.CONTACT_DETAILS, mContactSection);
                        }

                        startShippingAddressChangeNormalization(editedAddress);
                    }
                } else {
                    providePaymentInformation();
                }

                if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
            }
        });
    }

    private void editContact(final AutofillContact toEdit) {
        if (toEdit != null) {
            // Log the edit of a contact info.
            mJourneyLogger.incrementSelectionEdits(Section.CONTACT_INFO);
        }
        mContactEditor.edit(toEdit, new Callback<AutofillContact>() {
            @Override
            public void onResult(AutofillContact editedContact) {
                if (mUI == null) return;

                if (editedContact != null) {
                    mContactEditor.setPayerErrors(null);

                    // A partial or complete contact came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedContact.isComplete()) {
                        // If the contact is not complete according to the requirements of the flow,
                        // unselect it (editor can return incomplete information when cancelled).
                        mContactSection.setSelectedItemIndex(SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Contact is complete and we were in the "Add flow": add an item to the
                        // list.
                        mContactSection.addAndSelectItem(editedContact);
                    } else {
                        dispatchPayerDetailChangeEventIfNeeded(editedContact.toPayerDetail());
                    }
                    // If contact is complete and (toEdit != null), no action needed: the contact
                    // was already selected in the UI.
                }
                // If |editedContact| is null, the user has cancelled out of the "Add flow". No
                // action to take (if a contact was selected in the UI, it will stay selected).

                mUI.updateSection(PaymentRequestUI.DataType.CONTACT_DETAILS, mContactSection);

                if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
            }
        });
    }

    private void editCard(final AutofillPaymentInstrument toEdit) {
        if (toEdit != null) {
            // Log the edit of a credit card.
            mJourneyLogger.incrementSelectionEdits(Section.PAYMENT_METHOD);
        }
        mCardEditor.edit(toEdit, new Callback<AutofillPaymentInstrument>() {
            @Override
            public void onResult(AutofillPaymentInstrument editedCard) {
                if (mUI == null) return;

                if (editedCard != null) {
                    // A partial or complete card came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedCard.isComplete()) {
                        // If the card is not complete, unselect it (editor can return incomplete
                        // information when cancelled).
                        mPaymentMethodsSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Card is complete and we were in the "Add flow": add an item to the list.
                        mPaymentMethodsSection.addAndSelectItem(editedCard);
                    }
                    // If card is complete and (toEdit != null), no action needed: the card was
                    // already selected in the UI.
                }
                // If |editedCard| is null, the user has cancelled out of the "Add flow". No action
                // to take (if another card was selected prior to the add flow, it will stay
                // selected).

                updateInstrumentModifiedTotals();
                mUI.updateSection(
                        PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
            }
        });
    }

    @Override
    public void onInstrumentDetailsLoadingWithoutUI() {
        if (mClient == null || mUI == null || mPaymentResponseHelper == null) return;

        assert mPaymentMethodsSection.getSelectedItem() instanceof AutofillPaymentInstrument;

        mUI.showProcessingMessage();
    }

    @Override
    public boolean onPayClicked(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, EditableOption selectedPaymentMethod) {
        mInvokedPaymentInstrument = (PaymentInstrument) selectedPaymentMethod;

        EditableOption selectedContact =
                mContactSection != null ? mContactSection.getSelectedItem() : null;
        mPaymentResponseHelper = new PaymentResponseHelper(
                selectedShippingAddress, selectedShippingOption, selectedContact, this);

        // Create maps that are subsets of mMethodData and mModifiers, that contain
        // the payment methods supported by the selected payment instrument. If the
        // intersection of method data contains more than one payment method, the
        // payment app is at liberty to choose (or have the user choose) one of the
        // methods.
        Map<String, PaymentMethodData> methodData = new HashMap<>();
        Map<String, PaymentDetailsModifier> modifiers = new HashMap<>();
        boolean isGooglePaymentInstrument = false;
        for (String instrumentMethodName : mInvokedPaymentInstrument.getInstrumentMethodNames()) {
            if (mMethodData.containsKey(instrumentMethodName)) {
                methodData.put(instrumentMethodName, mMethodData.get(instrumentMethodName));
            }
            if (mModifiers != null && mModifiers.containsKey(instrumentMethodName)) {
                modifiers.put(instrumentMethodName, mModifiers.get(instrumentMethodName));
            }
            if (instrumentMethodName.equals(ANDROID_PAY_METHOD_NAME)
                    || instrumentMethodName.equals(PAY_WITH_GOOGLE_METHOD_NAME)) {
                isGooglePaymentInstrument = true;
            }
        }

        if (mInvokedPaymentInstrument instanceof ServiceWorkerPaymentApp) {
            if (mPaymentHandlerHost == null) {
                mPaymentHandlerHost = new PaymentHandlerHost(this /* delegate */);
            }

            ((ServiceWorkerPaymentApp) mInvokedPaymentInstrument)
                    .setPaymentHandlerHost(mPaymentHandlerHost);
        }

        mInvokedPaymentInstrument.invokePaymentApp(mId, mMerchantName, mTopLevelOrigin,
                mPaymentRequestOrigin, mCertificateChain, Collections.unmodifiableMap(methodData),
                mRawTotal, mRawLineItems, Collections.unmodifiableMap(modifiers), this);

        mJourneyLogger.setEventOccurred(Event.PAY_CLICKED);
        boolean isAutofillPaymentInstrument = mInvokedPaymentInstrument.isAutofillInstrument();
        // Record what type of instrument was selected when "Pay" was clicked.
        if (isAutofillPaymentInstrument) {
            mJourneyLogger.setEventOccurred(Event.SELECTED_CREDIT_CARD);
        } else if (isGooglePaymentInstrument) {
            mJourneyLogger.setEventOccurred(Event.SELECTED_GOOGLE);
        } else {
            mJourneyLogger.setEventOccurred(Event.SELECTED_OTHER);
        }
        return !isAutofillPaymentInstrument;
    }

    @Override
    public void onDismiss() {
        mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
        disconnectFromClientWithDebugMessage(ErrorStrings.USER_CANCELLED);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage) {
        disconnectFromClientWithDebugMessage(debugMessage, PaymentErrorReason.USER_CANCEL);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage, int reason) {
        Log.d(TAG, debugMessage);
        if (mClient != null) mClient.onError(reason, debugMessage);
        closeClient();
        closeUIAndDestroyNativeObjects(/*immediateClose=*/true);
        if (mNativeObserverForTest != null) mNativeObserverForTest.onConnectionTerminated();
    }

    /**
     * Called by the merchant website to abort the payment.
     */
    @Override
    public void abort() {
        if (mClient == null) return;

        if (mInvokedPaymentInstrument != null) {
            mInvokedPaymentInstrument.abortPaymentApp(mId, this);
            return;
        }
        onInstrumentAbortResult(true);
    }

    /** Called by the payment app in response to an abort request. */
    @Override
    public void onInstrumentAbortResult(boolean abortSucceeded) {
        mClient.onAbort(abortSucceeded);
        if (abortSucceeded) {
            closeClient();
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_MERCHANT);
            closeUIAndDestroyNativeObjects(/*immediateClose=*/true);
        } else {
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceUnableToAbort();
        }
        if (mNativeObserverForTest != null) mNativeObserverForTest.onAbortCalled();
    }

    /**
     * Called when the merchant website has processed the payment.
     */
    @Override
    public void complete(int result) {
        if (mClient == null) return;
        mJourneyLogger.setCompleted();
        if (!PaymentPreferencesUtil.isPaymentCompleteOnce()) {
            PaymentPreferencesUtil.setPaymentCompleteOnce();
        }
        assert mRawTotal != null;
        mJourneyLogger.recordTransactionAmount(
                mRawTotal.amount.currency, mRawTotal.amount.value, true /*completed*/);

        /**
         * Update records of the used payment instrument for sorting payment apps and instruments
         * next time.
         */
        EditableOption selectedPaymentMethod = mPaymentMethodsSection.getSelectedItem();
        PaymentPreferencesUtil.increasePaymentInstrumentUseCount(
                selectedPaymentMethod.getIdentifier());
        PaymentPreferencesUtil.setPaymentInstrumentLastUseDate(
                selectedPaymentMethod.getIdentifier(), System.currentTimeMillis());

        closeUIAndDestroyNativeObjects(/*immediateClose=*/PaymentComplete.FAIL != result);
    }

    @Override
    public void retry(PaymentValidationErrors errors) {
        if (mClient == null) return;

        if (!PaymentValidator.validatePaymentValidationErrors(errors)) {
            mJourneyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_VALIDATION_ERRORS);
            return;
        }

        mWasRetryCalled = true;

        // Go back to the payment sheet
        mUI.onPayButtonProcessingCancelled();
        if (!TextUtils.isEmpty(errors.error)) {
            mUI.setRetryErrorMessage(errors.error);
        } else {
            ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
            mUI.setRetryErrorMessage(
                    activity.getResources().getString(R.string.payments_error_message));
        }

        if (mRequestShipping && hasShippingAddressError(errors.shippingAddress)) {
            mRetryQueue.add(() -> {
                mAddressEditor.setAddressErrors(errors.shippingAddress);
                AutofillAddress selectedAddress =
                        (AutofillAddress) mShippingAddressesSection.getSelectedItem();
                editAddress(selectedAddress);
            });
        }

        if ((mRequestPayerName || mRequestPayerPhone || mRequestPayerEmail)
                && hasPayerError(errors.payer)) {
            mRetryQueue.add(() -> {
                mContactEditor.setPayerErrors(errors.payer);
                AutofillContact selectedContact =
                        (AutofillContact) mContactSection.getSelectedItem();
                editContact(selectedContact);
            });
        }

        if (!mRetryQueue.isEmpty()) mHandler.post(mRetryQueue.remove());
    }

    private boolean hasShippingAddressError(AddressErrors errors) {
        return !TextUtils.isEmpty(errors.addressLine) || !TextUtils.isEmpty(errors.city)
                || !TextUtils.isEmpty(errors.country)
                || !TextUtils.isEmpty(errors.dependentLocality)
                || !TextUtils.isEmpty(errors.organization) || !TextUtils.isEmpty(errors.phone)
                || !TextUtils.isEmpty(errors.postalCode) || !TextUtils.isEmpty(errors.recipient)
                || !TextUtils.isEmpty(errors.region) || !TextUtils.isEmpty(errors.sortingCode);
    }

    private boolean hasPayerError(PayerErrors errors) {
        return !TextUtils.isEmpty(errors.name) || !TextUtils.isEmpty(errors.phone)
                || !TextUtils.isEmpty(errors.email);
    }

    @Override
    public void onCardAndAddressSettingsClicked() {
        Context context = ChromeActivity.fromWebContents(mWebContents);
        if (context == null) {
            mJourneyLogger.setAborted(AbortReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        PreferencesLauncher.launchSettingsPageCompat(context, MainPreferences.class);
    }

    @Override
    public void onAddressUpdated(AutofillAddress address) {
        if (mClient == null) return;

        address.setShippingAddressLabelWithCountry();
        mCardEditor.updateBillingAddressIfComplete(address);

        if (mShippingAddressesSection != null) {
            mShippingAddressesSection.addAndSelectOrUpdateItem(address);
            mUI.updateSection(
                    PaymentRequestUI.DataType.SHIPPING_ADDRESSES, mShippingAddressesSection);
        }

        if (mContactSection != null) {
            mContactSection.addOrUpdateWithAutofillAddress(address);
            mUI.updateSection(PaymentRequestUI.DataType.CONTACT_DETAILS, mContactSection);
        }
    }

    @Override
    public void onAddressDeleted(String guid) {
        if (mClient == null) return;

        // TODO: Delete the address from mShippingAddressesSection and mContactSection. Note that we
        // only displayed SUGGESTIONS_LIMIT addresses, so we may want to add back previously
        // ignored addresses.
    }

    @Override
    public void onCreditCardUpdated(CreditCard card) {
        if (mClient == null) return;
        if (!mMerchantSupportsAutofillPaymentInstruments || mPaymentMethodsSection == null) return;

        PaymentInstrument updatedAutofillPaymentInstruments = null;
        for (PaymentApp app : mApps) {
            if (app instanceof AutofillPaymentApp) {
                updatedAutofillPaymentInstruments =
                        ((AutofillPaymentApp) app).getInstrumentForCard(card);
            }
        }
        if (updatedAutofillPaymentInstruments == null) return;

        mPaymentMethodsSection.addAndSelectOrUpdateItem(updatedAutofillPaymentInstruments);

        updateInstrumentModifiedTotals();

        if (mUI != null) {
            mUI.updateSection(PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
        }
    }

    @Override
    public void onCreditCardDeleted(String guid) {
        if (mClient == null) return;
        if (!mMerchantSupportsAutofillPaymentInstruments || mPaymentMethodsSection == null) return;

        mPaymentMethodsSection.removeAndUnselectItem(guid);

        updateInstrumentModifiedTotals();

        if (mUI != null) {
            mUI.updateSection(PaymentRequestUI.DataType.PAYMENT_METHODS, mPaymentMethodsSection);
        }
    }

    /**
     * Called by the merchant website to check if the user has complete payment instruments.
     */
    @Override
    public void canMakePayment(boolean legacyMode) {
        if (mClient == null) return;

        if (mNativeObserverForTest != null) mNativeObserverForTest.onCanMakePaymentCalled();

        if (isFinishedQueryingPaymentApps()) {
            respondCanMakePaymentQuery(legacyMode);
        } else {
            mIsCanMakePaymentResponsePending = true;
            mUseLegacyCanMakePayment = legacyMode;
        }
    }

    private void respondCanMakePaymentQuery(boolean legacyMode) {
        if (mClient == null) return;

        mIsCanMakePaymentResponsePending = false;

        boolean response = legacyMode ? mHasEnrolledInstrument : mArePaymentMethodsSupported;
        response &= mDelegate.prefsCanMakePayment();

        // Only need to enforce query quota in legacy mode. Per-method quota not supported.
        if (legacyMode
                && !CanMakePaymentQuery.canQuery(mWebContents, mTopLevelOrigin,
                        mPaymentRequestOrigin, mMethodData, /*perMethodQuota=*/false)) {
            if (shouldEnforceCanMakePaymentQueryQuota()) {
                mClient.onCanMakePayment(CanMakePaymentQueryResult.QUERY_QUOTA_EXCEEDED);
            } else {
                mClient.onCanMakePayment(response
                                ? CanMakePaymentQueryResult.WARNING_CAN_MAKE_PAYMENT
                                : CanMakePaymentQueryResult.WARNING_CANNOT_MAKE_PAYMENT);
            }
        } else {
            mClient.onCanMakePayment(response ? CanMakePaymentQueryResult.CAN_MAKE_PAYMENT
                                              : CanMakePaymentQueryResult.CANNOT_MAKE_PAYMENT);
        }

        mJourneyLogger.setCanMakePaymentValue(response || mIsIncognito);

        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceCanMakePaymentQueryResponded();
        }
        if (mNativeObserverForTest != null) mNativeObserverForTest.onCanMakePaymentReturned();
    }

    /**
     * Called by the merchant website to check if the user has complete payment instruments.
     */
    @Override
    public void hasEnrolledInstrument(boolean perMethodQuota) {
        if (mClient == null) return;

        if (mNativeObserverForTest != null) mNativeObserverForTest.onHasEnrolledInstrumentCalled();

        mHasEnrolledInstrumentUsesPerMethodQuota = perMethodQuota;

        if (isFinishedQueryingPaymentApps()) {
            respondHasEnrolledInstrumentQuery(mHasEnrolledInstrument);
        } else {
            mIsHasEnrolledInstrumentResponsePending = true;
        }
    }

    private void respondHasEnrolledInstrumentQuery(boolean response) {
        if (mClient == null) return;

        mIsHasEnrolledInstrumentResponsePending = false;

        if (CanMakePaymentQuery.canQuery(mWebContents, mTopLevelOrigin, mPaymentRequestOrigin,
                    mMethodData, mHasEnrolledInstrumentUsesPerMethodQuota)) {
            mClient.onHasEnrolledInstrument(response
                            ? HasEnrolledInstrumentQueryResult.HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.HAS_NO_ENROLLED_INSTRUMENT);
        } else if (shouldEnforceCanMakePaymentQueryQuota()) {
            mClient.onHasEnrolledInstrument(HasEnrolledInstrumentQueryResult.QUERY_QUOTA_EXCEEDED);
        } else {
            mClient.onHasEnrolledInstrument(response
                            ? HasEnrolledInstrumentQueryResult.WARNING_HAS_ENROLLED_INSTRUMENT
                            : HasEnrolledInstrumentQueryResult.WARNING_HAS_NO_ENROLLED_INSTRUMENT);
        }

        mJourneyLogger.setHasEnrolledInstrumentValue(response || mIsIncognito);

        if (sObserverForTest != null) {
            sObserverForTest.onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();
        }
        if (mNativeObserverForTest != null) {
            mNativeObserverForTest.onHasEnrolledInstrumentReturned();
        }
    }

    /**
     * @return Whether canMakePayment() query quota should be enforced. By default, the quota is
     * enforced only on https:// scheme origins. However, the tests also enable the quota on
     * localhost and file:// scheme origins to verify its behavior.
     */
    private boolean shouldEnforceCanMakePaymentQueryQuota() {
        // If |mWebContents| is destroyed, don't bother checking the localhost or file:// scheme
        // exemption. It doesn't really matter anyways.
        return mWebContents.isDestroyed()
                || !UrlUtil.isLocalDevelopmentUrl(mWebContents.getLastCommittedUrl())
                || sIsLocalCanMakePaymentQueryQuotaEnforcedForTest;
    }

    /**
     * Called when the renderer closes the Mojo connection.
     */
    @Override
    public void close() {
        if (mClient == null) return;
        closeClient();
        mJourneyLogger.setAborted(AbortReason.MOJO_RENDERER_CLOSING);
        closeUIAndDestroyNativeObjects(/*immediateClose=*/true);
        if (mNativeObserverForTest != null) mNativeObserverForTest.onConnectionTerminated();
    }

    /**
     * Called when the Mojo connection encounters an error.
     */
    @Override
    public void onConnectionError(MojoException e) {
        if (mClient == null) return;
        closeClient();
        mJourneyLogger.setAborted(AbortReason.MOJO_CONNECTION_ERROR);
        closeUIAndDestroyNativeObjects(/*immediateClose=*/true);
        if (mNativeObserverForTest != null) mNativeObserverForTest.onConnectionTerminated();
    }

    /**
     * Called after retrieving the list of payment instruments in an app.
     */
    @Override
    public void onInstrumentsReady(PaymentApp app, List<PaymentInstrument> instruments) {
        if (mClient == null) return;
        mPendingApps.remove(app);

        if (instruments != null) {
            for (int i = 0; i < instruments.size(); i++) {
                PaymentInstrument instrument = instruments.get(i);
                Set<String> instrumentMethodNames =
                        new HashSet<>(instrument.getInstrumentMethodNames());
                instrumentMethodNames.retainAll(mMethodData.keySet());
                if (!instrumentMethodNames.isEmpty()) {
                    mHideServerAutofillInstruments |=
                            instrument.isServerAutofillInstrumentReplacement();
                    mHasEnrolledInstrument |= instrument.canMakePayment();
                    mPendingInstruments.add(instrument);
                } else {
                    instrument.dismissInstrument();
                }
            }
        }

        // Always return false when can make payment is disabled.
        mHasEnrolledInstrument &= mDelegate.prefsCanMakePayment();

        int additionalTextResourceId = app.getAdditionalAppTextResourceId();
        if (additionalTextResourceId != 0) {
            assert mPaymentMethodsSectionAdditionalTextResourceId == 0;
            assert app instanceof AutofillPaymentApp;
            mPaymentMethodsSectionAdditionalTextResourceId = additionalTextResourceId;
        }

        // Some payment apps still have not responded. Continue waiting for them.
        if (!mPendingApps.isEmpty()) return;

        if (disconnectIfNoPaymentMethodsSupported()) return;

        if (mHideServerAutofillInstruments) {
            List<PaymentInstrument> nonServerAutofillInstruments = new ArrayList<>();
            for (int i = 0; i < mPendingInstruments.size(); i++) {
                if (!mPendingInstruments.get(i).isServerAutofillInstrument()) {
                    nonServerAutofillInstruments.add(mPendingInstruments.get(i));
                }
            }
            mPendingInstruments = nonServerAutofillInstruments;
        }

        // Load the validation rules for each unique region code in the credit card billing
        // addresses and check for validity.
        Set<String> uniqueCountryCodes = new HashSet<>();
        for (int i = 0; i < mPendingInstruments.size(); ++i) {
            @Nullable
            String countryCode = mPendingInstruments.get(i).getCountryCode();
            if (countryCode != null && !uniqueCountryCodes.contains(countryCode)) {
                uniqueCountryCodes.add(countryCode);
                PersonalDataManager.getInstance().loadRulesForAddressNormalization(countryCode);
            }
        }

        Collections.sort(mPendingInstruments, PAYMENT_INSTRUMENT_COMPARATOR);

        // Possibly pre-select the first instrument on the list.
        int selection = !mPendingInstruments.isEmpty() && mPendingInstruments.get(0).canPreselect()
                ? 0
                : SectionInformation.NO_SELECTION;

        if (mIsCanMakePaymentResponsePending) {
            respondCanMakePaymentQuery(mUseLegacyCanMakePayment);
        }

        if (mIsHasEnrolledInstrumentResponsePending) {
            respondHasEnrolledInstrumentQuery(mHasEnrolledInstrument);
        }

        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);
        if (chromeActivity == null) {
            mJourneyLogger.setNotShown(NotShownReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
            return;
        }

        // The list of payment instruments is ready to display.
        mPaymentMethodsSection = new SectionInformation(PaymentRequestUI.DataType.PAYMENT_METHODS,
                selection, new ArrayList<>(mPendingInstruments));
        if (mPaymentMethodsSectionAdditionalTextResourceId != 0) {
            mPaymentMethodsSection.setAdditionalText(
                    chromeActivity.getString(mPaymentMethodsSectionAdditionalTextResourceId));
        }

        // Record the number suggested payment methods and whether at least one of them was
        // complete.
        mJourneyLogger.setNumberOfSuggestionsShown(Section.PAYMENT_METHOD,
                mPendingInstruments.size(),
                !mPendingInstruments.isEmpty() && mPendingInstruments.get(0).isComplete());

        int missingFields = 0;
        if (mPendingInstruments.isEmpty()) {
            if (mMerchantSupportsAutofillPaymentInstruments) {
                // Record all fields if basic-card is supported but no card exists.
                missingFields = AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_EXPIRED
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_CARDHOLDER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_NUMBER
                        | AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS;
            }
        } else if (mPendingInstruments.get(0).isAutofillInstrument()) {
            missingFields =
                    ((AutofillPaymentInstrument) (mPendingInstruments.get(0))).getMissingFields();
        }
        if (missingFields != 0) {
            RecordHistogram.recordSparseHistogram(
                    "PaymentRequest.MissingPaymentFields", missingFields);
        }

        mPendingInstruments.clear();

        updateInstrumentModifiedTotals();

        // UI has requested the full list of payment instruments. Provide it now.
        if (mPaymentInformationCallback != null && !mWaitForUpdatedDetails) {
            providePaymentInformation();
        }

        SettingsAutofillAndPaymentsObserver.getInstance().registerObserver(this);

        triggerPaymentAppUiSkipIfApplicable(chromeActivity);
    }

    /**
     * If no payment methods are supported, disconnect from the client and return true.
     *
     * @return True if no payment methods are supported
     */
    private boolean disconnectIfNoPaymentMethodsSupported() {
        if (!isFinishedQueryingPaymentApps() || !mIsCurrentPaymentRequestShowing) return false;

        boolean foundPaymentMethods =
                mPaymentMethodsSection != null && !mPaymentMethodsSection.isEmpty();

        if (!mArePaymentMethodsSupported || (!foundPaymentMethods && !mUserCanAddCreditCard)) {
            // All payment apps have responded, but none of them have instruments. It's possible to
            // add credit cards, but the merchant does not support them either. The payment request
            // must be rejected.
            mJourneyLogger.setNotShown(mArePaymentMethodsSupported
                            ? NotShownReason.NO_MATCHING_PAYMENT_METHOD
                            : NotShownReason.NO_SUPPORTED_PAYMENT_METHOD);
            if (mIsProhibitedOriginOrInvalidSsl) {
                if (mNativeObserverForTest != null) mNativeObserverForTest.onNotSupportedError();
                // Chrome always refuses payments with invalid SSL and in prohibited origin types.
                disconnectFromClientWithDebugMessage(
                        mRejectShowErrorMessage, PaymentErrorReason.NOT_SUPPORTED);
            } else if (mIsIncognito) {
                // If the user is in the incognito mode, hide the absence of their payment methods
                // from the merchant site.
                disconnectFromClientWithDebugMessage(
                        ErrorStrings.USER_CANCELLED, PaymentErrorReason.USER_CANCEL);
            } else {
                if (mNativeObserverForTest != null) mNativeObserverForTest.onNotSupportedError();
                disconnectFromClientWithDebugMessage(
                        ErrorStrings.GENERIC_PAYMENT_METHOD_NOT_SUPPORTED_MESSAGE
                                + (TextUtils.isEmpty(mRejectShowErrorMessage)
                                                ? ""
                                                : " " + mRejectShowErrorMessage),
                        PaymentErrorReason.NOT_SUPPORTED);
            }
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
            return true;
        }

        return false;
    }

    /** @return True after payment apps have been queried. */
    private boolean isFinishedQueryingPaymentApps() {
        return mPendingApps != null && mPendingApps.isEmpty() && mPendingInstruments.isEmpty();
    }

    /**
     * Called after retrieving instrument details.
     */
    @Override
    public void onInstrumentDetailsReady(String methodName, String stringifiedDetails) {
        assert methodName != null;
        assert stringifiedDetails != null;

        if (mClient == null || mPaymentResponseHelper == null) return;

        // If the payment method was an Autofill credit card with an identifier, record its use.
        EditableOption selectedPaymentMethod = mPaymentMethodsSection.getSelectedItem();
        if (selectedPaymentMethod instanceof AutofillPaymentInstrument
                && !selectedPaymentMethod.getIdentifier().isEmpty()) {
            PersonalDataManager.getInstance().recordAndLogCreditCardUse(
                    selectedPaymentMethod.getIdentifier());
        }

        // Showing the payment request UI if we were previously skipping it so the loading
        // spinner shows up until the merchant notifies that payment was completed.
        if (mShouldSkipShowingPaymentRequestUi) mUI.showProcessingMessageAfterUiSkip();

        mJourneyLogger.setEventOccurred(Event.RECEIVED_INSTRUMENT_DETAILS);

        mPaymentResponseHelper.onInstrumentDetailsReceived(methodName, stringifiedDetails);
    }

    @Override
    public void onPaymentResponseReady(PaymentResponse response) {
        mClient.onPaymentResponse(response);
        mPaymentResponseHelper = null;
        if (sObserverForTest != null) sObserverForTest.onPaymentResponseReady();
    }

    /**
     * Called if unable to retrieve instrument details.
     */
    @Override
    public void onInstrumentDetailsError(String errorMessage) {
        if (mClient == null) return;
        mInvokedPaymentInstrument = null;
        // When skipping UI, any errors/cancel from fetching instrument details should abort
        // payment.
        if (mShouldSkipShowingPaymentRequestUi) {
            assert !TextUtils.isEmpty(errorMessage);
            mJourneyLogger.setAborted(AbortReason.ABORTED_BY_USER);
            disconnectFromClientWithDebugMessage(errorMessage);
        } else {
            mUI.onPayButtonProcessingCancelled();
        }
    }

    @Override
    public void onFocusChanged(@PaymentRequestUI.DataType int dataType, boolean willFocus) {
        assert dataType == PaymentRequestUI.DataType.SHIPPING_ADDRESSES;

        if (mShippingAddressesSection.getSelectedItem() == null) return;

        AutofillAddress selectedAddress =
                (AutofillAddress) mShippingAddressesSection.getSelectedItem();

        // The label should only include the country if the view is focused.
        if (willFocus) {
            selectedAddress.setShippingAddressLabelWithCountry();
        } else {
            selectedAddress.setShippingAddressLabelWithoutCountry();
        }

        mUI.updateSection(PaymentRequestUI.DataType.SHIPPING_ADDRESSES, mShippingAddressesSection);
    }

    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        ChromeActivity chromeActivity = ChromeActivity.fromWebContents(mWebContents);

        // Can happen if the tab is closed during the normalization process.
        if (chromeActivity == null) {
            mJourneyLogger.setAborted(AbortReason.OTHER);
            disconnectFromClientWithDebugMessage(ErrorStrings.ACTIVITY_NOT_FOUND);
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceShowFailed();
            return;
        }

        // Don't reuse the selected address because it is formatted for display.
        AutofillAddress shippingAddress = new AutofillAddress(chromeActivity, profile);

        // Redact shipping address before exposing it in ShippingAddressChangeEvent.
        // https://w3c.github.io/payment-request/#shipping-address-changed-algorithm
        PaymentAddress redactedAddress = shippingAddress.toPaymentAddress();
        if (PaymentsExperimentalFeatures.isEnabled(
                    ChromeFeatureList.WEB_PAYMENTS_REDACT_SHIPPING_ADDRESS)) {
            redactedAddress.organization = "";
            redactedAddress.phone = "";
            redactedAddress.recipient = "";
            redactedAddress.addressLine = new String[0];
        }

        // This updates the line items and the shipping options asynchronously.
        mClient.onShippingAddressChange(redactedAddress);
    }

    @Override
    public void onCouldNotNormalize(AutofillProfile profile) {
        // Since the phone number is formatted in either case, this profile should be used.
        onAddressNormalized(profile);
    }

    /**
     * Starts the normalization of the new shipping address. Will call back into either
     * onAddressNormalized or onCouldNotNormalize which will send the result to the merchant.
     */
    private void startShippingAddressChangeNormalization(AutofillAddress address) {
        PersonalDataManager.getInstance().normalizeAddress(address.getProfile(), this);
    }

    /**
     * Closes the UI and destroys native objects. If the client is still connected, then it's
     * notified of UI hiding. This PaymentRequestImpl object can't be reused after this function is
     * called.
     *
     * @param immediateClose If true, then UI immediately closes. If false, the UI shows the error
     *                       message "There was an error processing your order." This message
     *                       implies that the merchant attempted to process the order, failed, and
     *                       called complete("fail") to notify the user. Therefore, this parameter
     *                       may be "false" only when called from
     *                       {@link PaymentRequestImpl#complete(int)}. All other callers should
     *                       always pass "true."
     */
    private void closeUIAndDestroyNativeObjects(boolean immediateClose) {
        if (mUI != null) {
            mUI.close(immediateClose, () -> {
                if (mClient != null) mClient.onComplete();
                closeClient();
            });
            mUI = null;
            mIsCurrentPaymentRequestShowing = false;
            setIsAnyPaymentRequestShowing(false);
        }

        if (mPaymentMethodsSection != null) {
            for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
                EditableOption option = mPaymentMethodsSection.getItem(i);
                ((PaymentInstrument) option).dismissInstrument();
            }
            mPaymentMethodsSection = null;
        }

        if (mObservedTabModelSelector != null) {
            mObservedTabModelSelector.removeObserver(mSelectorObserver);
            mObservedTabModelSelector = null;
        }

        if (mObservedTabModel != null) {
            mObservedTabModel.removeObserver(mTabModelObserver);
            mObservedTabModel = null;
        }

        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }

        SettingsAutofillAndPaymentsObserver.getInstance().unregisterObserver(this);

        // Destroy native objects.
        for (CurrencyFormatter formatter : mCurrencyFormatterMap.values()) {
            assert formatter != null;
            // Ensures the native implementation of currency formatter does not leak.
            formatter.destroy();
        }
        mJourneyLogger.destroy();

        if (mPaymentHandlerHost != null) mPaymentHandlerHost.destroy();
    }

    private void closeClient() {
        if (mClient != null) mClient.close();
        mClient = null;
    }

    private void dispatchPayerDetailChangeEventIfNeeded(PayerDetail detail) {
        if (mClient == null || !mWasRetryCalled) return;
        mClient.onPayerDetailChange(detail);
    }

    /**
     * @return Whether any instance of PaymentRequest has received a show() call. Don't use this
     *         function to check whether the current instance has received a show() call.
     */
    private static boolean getIsAnyPaymentRequestShowing() {
        return sIsAnyPaymentRequestShowing;
    }

    /** @param isShowing Whether any instance of PaymentRequest has received a show() call. */
    private static void setIsAnyPaymentRequestShowing(boolean isShowing) {
        sIsAnyPaymentRequestShowing = isShowing;
    }

    @VisibleForTesting
    public static void setObserverForTest(PaymentRequestServiceObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }

    @VisibleForTesting
    public static void setIsLocalCanMakePaymentQueryQuotaEnforcedForTest() {
        sIsLocalCanMakePaymentQueryQuotaEnforcedForTest = true;
    }

    @VisibleForTesting
    /* package */ void setSkipUIForNonURLPaymentMethodIdentifiersForTest() {
        calculateWhetherShouldSkipShowingPaymentRequestUi(
                true /* skipUiForNonUrlPaymentMethodIdentifiersForTest */);
    }

    /**
     * Compares two payment instruments by frecency.
     * Return negative value if a has strictly lower frecency score than b.
     * Return zero if a and b have the same frecency score.
     * Return positive value if a has strictly higher frecency score than b.
     */
    private static int compareInstrumentsByFrecency(PaymentInstrument a, PaymentInstrument b) {
        int aCount = PaymentPreferencesUtil.getPaymentInstrumentUseCount(a.getIdentifier());
        int bCount = PaymentPreferencesUtil.getPaymentInstrumentUseCount(b.getIdentifier());
        long aDate = PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(a.getIdentifier());
        long bDate = PaymentPreferencesUtil.getPaymentInstrumentLastUseDate(a.getIdentifier());

        return Double.compare(getFrecencyScore(aCount, aDate), getFrecencyScore(bCount, bDate));
    }

    /**
     * Compares two Completable by completeness score.
     * Return negative value if a has strictly lower completeness score than b.
     * Return zero if a and b have the same completeness score.
     * Return positive value if a has strictly higher completeness score than b.
     */
    private static int compareCompletablesByCompleteness(Completable a, Completable b) {
        return Integer.compare(a.getCompletenessScore(), b.getCompletenessScore());
    }

    /**
     * The frecency score is calculated according to use count and last use date. The formula is
     * the same as the one used in GetFrecencyScore in autofill_data_model.cc.
     */
    private static final double getFrecencyScore(int count, long date) {
        long currentTime = System.currentTimeMillis();
        return -Math.log((currentTime - date) / (24 * 60 * 60 * 1000) + 2) / Math.log(count + 2);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Supervision Onboarding polymer element. It loads onboarding
 * pages from the web in a webview and forwards user actions to the
 * OnboardingController, which contains the full flow state machine.
 */

{
  /**
   * Codes for network errors. Should follow same numbers as
   * net/base/net_errors.h
   * @const
   * @enum {number}
   */
  const NetError = {
    'OK': 0,
    'ERR_FAILED': -2,
    'ERR_ABORTED': -3,
    'ERR_TIMED_OUT': -7,
  };

  /** @const {number} Timeout for page loads in milliseconds. */
  const PAGE_LOAD_TIMEOUT_MS = 10000;

  class WebviewLoader {
    /** @param {!WebView} webview */
    constructor(webview) {
      this.webview_ = webview;

      /**
       * Page being currently loaded. If null we are not loading any page yet.
       * @private {?chromeos.supervision.mojom.OnboardingPage}
       */
      this.page_ = null;

      /**
       * Pending callback for a webview page load. It will be called with the
       * list of custom header values if asked by the controller, or an empty
       * array otherwise.
       * @private{?function({result:
       *     !chromeos.supervision.mojom.OnboardingLoadPageResult})}
       */
      this.pendingLoadPageCallback_ = null;

      /**
       * Callers can optionally ask to extract the value from a custom HTTP
       * header present in page loads. If they request it and we find the
       * targeted header, we store it here.
       * @private {?string}
       */
      this.customHeaderValue_ = null;

      // We listen to all requests made to fetch the main frame, but note that
      // we end up blocking requests to URLs that don't start with the expected
      // server prefix (see onBeforeSendHeaders_).
      // This is done because we will only know the prefix when we start to
      // load the page, but we add listeners at mojo setup time.
      const requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};

      this.webview_.request.onBeforeSendHeaders.addListener(
          this.onBeforeSendHeaders_.bind(this), requestFilter,
          ['blocking', 'requestHeaders']);
      this.webview_.request.onHeadersReceived.addListener(
          this.onHeadersReceived_.bind(this), requestFilter,
          ['responseHeaders', 'extraHeaders']);

      // These are for listeners that will be called when we finish loading the
      // webview. We have their bound versions here so we can add and remove
      // them as we try to load the page. This is necessary so we don't receive
      // notifications from previous loads.
      // For example, if we timeout and the user presses the retry button, the
      // webview will send a loadabort event when we set a new src and start a
      // new page load. We avoid this problem if we remove our listeners as soon
      // as one of them fires.
      this.loadStopListener_ = this.onLoadStop_.bind(this);
      this.requestCompletedListener_ = this.onRequestCompleted_.bind(this);
      this.loadAbortListener_ = this.onLoadAbort_.bind(this);
      this.errorOccurredListener_ = this.onErrorOccurred_.bind(this);
      this.timeoutListener_ = this.onTimeout_.bind(this);

      this.timerId_ = 0;
    }

    /**
     * @param {!chromeos.supervision.mojom.OnboardingPage} page
     * @return {Promise<{
     *     result: !chromeos.supervision.mojom.OnboardingLoadPageResult,
     *  }>}
     */
    loadPage(page) {
      if (this.pendingLoadPageCallback_) {
        this.finishLoading_({netError: NetError.ERR_ABORTED});
      }

      this.page_ = page;
      this.pendingLoadPageCallback_ = null;
      this.customHeaderValue_ = null;

      this.webview_.addEventListener('loadstop', this.loadStopListener_);
      this.webview_.addEventListener('loadabort', this.loadAbortListener_);
      this.webview_.request.onCompleted.addListener(
          this.requestCompletedListener_,
          {urls: ['<all_urls>'], types: ['main_frame']});
      this.webview_.request.onErrorOccurred.addListener(
          this.errorOccurredListener_,
          {urls: ['<all_urls>'], types: ['main_frame']});
      this.timerId_ =
          window.setTimeout(this.timeoutListener_, PAGE_LOAD_TIMEOUT_MS);

      this.webview_.src = page.url.url;

      return new Promise(resolve => {
        this.pendingLoadPageCallback_ = resolve;
      });
    }

    /**
     * Injects headers into the passed request.
     *
     * @param {!Object} requestEvent
     * @return {!BlockingResponse} Modified headers.
     * @private
     */
    onBeforeSendHeaders_(requestEvent) {
      if (!this.page_ ||
          !requestEvent.url.startsWith(this.page_.allowedUrlsPrefix)) {
        return {cancel: true};
      }

      requestEvent.requestHeaders.push(
          {name: 'Authorization', value: 'Bearer ' + this.page_.accessToken});

      return /** @type {!BlockingResponse} */ ({
        requestHeaders: requestEvent.requestHeaders,
      });
    }

    /**
     * @param {!Object<{responseHeaders: !chrome.webRequest.HttpHeaders}>}
     *     responseEvent
     * @return {!BlockingResponse}
     * @private
     */
    onHeadersReceived_(responseEvent) {
      if (!this.page_.customHeaderName) {
        return {};
      }

      const header = responseEvent.responseHeaders.find(
          h => h.name.toUpperCase() ==
              this.page_.customHeaderName.toUpperCase());
      if (header) {
        this.customHeaderValue_ = header.value;
      }

      return {};
    }

    /**
     * Called when the webview stops loading.
     * @param {!Event} e
     * @private
     */
    onLoadStop_(e) {
      // If we got here, this means that all the error handling listeners did
      // not fire, so just return a success result.
      const result = {netError: NetError.OK};
      if (this.customHeaderValue_) {
        result.customHeaderValue = this.customHeaderValue_;
      }
      this.finishLoading_(result);
    }

    /**
     * Called when the webview sends a loadabort event.
     * @param {!Event} e
     * @private
     */
    onLoadAbort_(e) {
      const netError = e.code ? e.code : NetError.ERR_FAILED;
      this.finishLoading_({netError: netError});
    }

    /**
     * Called when the webview completes a page request.
     * @param {!Object} requestDetails
     * @private
     */
    onRequestCompleted_(requestDetails) {
      if (!requestDetails.statusCode || requestDetails.statusCode == 200) {
        // We don't finish loading with a success here because at this point the
        // webview has finished the request, but it didn't render the results
        // yet. If we call finishLoading and start showing the webview, we will
        // flicker with the old results.
        return;
      }

      // HTTP errors in the 4xx and 5xx range will hit here.
      this.finishLoading_({netError: NetError.ERR_FAILED});
    }

    /**
     * Called when the webview encounters an error.
     * @private
     */
    onErrorOccurred_() {
      this.finishLoading_({netError: NetError.ERR_FAILED});
    }

    /**
     * Called when the webview load times out.
     * @private
     */
    onTimeout_() {
      this.finishLoading_({netError: NetError.ERR_TIMED_OUT});
    }

    /**
     * Called to clean up the webview listeners and run the pending callback
     * with the given result.
     * @param {!chromeos.supervision.mojom.OnboardingLoadPageResult} result
     */
    finishLoading_(result) {
      this.webview_.removeEventListener('loadstop', this.loadStopListener_);
      this.webview_.removeEventListener('loadabort', this.loadAbortListener_);
      this.webview_.request.onCompleted.removeListener(
          this.requestCompletedListener_);
      this.webview_.request.onErrorOccurred.removeListener(
          this.errorOccurredListener_);
      window.clearTimeout(this.timerId_);

      this.pendingLoadPageCallback_({result: result});
    }
  }

  Polymer({
    is: 'supervision-onboarding',

    behaviors: [LoginScreenBehavior, OobeDialogHostBehavior],

    properties: {
      /*
       * Properties to hide the main dialogs used to present the flow. Only one
       * of them can be shown at a time.
       */
      hideContent_: {type: Boolean, value: true},
      hideLoadingDialog_: {type: Boolean, value: true},
      hideRetryDialog_: {type: Boolean, value: true},

      /*
       * Properties to hide the buttons that can trigger flow actions.
       */
      hideBackButton_: {type: Boolean, value: true},
      hideSkipButton_: {type: Boolean, value: true},
      hideNextButton_: {type: Boolean, value: true},
      hideRetryButton_: {type: Boolean, value: true},
    },

    /** Overridden from LoginScreenBehavior. */
    EXTERNAL_API: [
      'setupMojo',
    ],

    /** @private {?chromeos.supervision.mojom.OnboardingControllerProxy} */
    controller_: null,

    /**
     * @private {?chromeos.supervision.mojom.
     *     OnboardingWebviewHostCallbackRouter}
     */
    hostCallbackRouter_: null,

    /** @private {?WebviewLoader} */
    webviewLoader_: null,

    setupMojo: function() {
      this.webviewLoader_ =
          new WebviewLoader(this.$.supervisionOnboardingWebview);

      this.controller_ =
          chromeos.supervision.mojom.OnboardingController.getProxy();

      this.hostCallbackRouter_ =
          new chromeos.supervision.mojom.OnboardingWebviewHostCallbackRouter();

      this.hostCallbackRouter_.setPresentation.addListener(
          this.setPresentation_.bind(this));
      this.hostCallbackRouter_.loadPage.addListener(
          this.webviewLoader_.loadPage.bind(this.webviewLoader_));

      this.controller_.bindWebviewHost(this.hostCallbackRouter_.createProxy());
    },

    /** @override */
    ready: function() {
      this.initializeLoginScreen('SupervisionOnboardingScreen', {
        resetAllowed: true,
      });
    },

    /**
     * @param {!chromeos.supervision.mojom.OnboardingPresentation} presentation
     * @private
     */
    setPresentation_: function(presentation) {
      this.hideContent_ = presentation.state !=
          chromeos.supervision.mojom.OnboardingPresentationState.kReady;
      this.hideRetryDialog_ = presentation.state !=
          chromeos.supervision.mojom.OnboardingPresentationState
              .kPageLoadFailed;
      this.hideLoadingDialog_ = presentation.state !=
          chromeos.supervision.mojom.OnboardingPresentationState.kLoading;

      this.hideBackButton_ = !presentation.canShowPreviousPage;
      this.hideSkipButton_ = !presentation.canSkipFlow;
      this.hideNextButton_ = !presentation.canShowNextPage;
      this.hideRetryButton_ = !presentation.canRetryPageLoad;
    },

    /** @private */
    onBack_: function() {
      this.controller_.handleAction(
          chromeos.supervision.mojom.OnboardingAction.kShowPreviousPage);
    },

    /** @private */
    onSkip_: function() {
      this.controller_.handleAction(
          chromeos.supervision.mojom.OnboardingAction.kSkipFlow);
    },

    /** @private */
    onNext_: function() {
      this.controller_.handleAction(
          chromeos.supervision.mojom.OnboardingAction.kShowNextPage);
    },

    /** @private */
    onRetry_: function() {
      this.controller_.handleAction(
          chromeos.supervision.mojom.OnboardingAction.kRetryPageLoad);
    },
  });
}

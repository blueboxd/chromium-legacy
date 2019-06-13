// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Supervision Onboarding polymer element. It loads onboarding
 * pages from the web in a webview and forwards user actions to the
 * OnboardingController, which contains the full flow state machine.
 */

{
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

      /** @private {!chromeos.supervision.mojom.OnboardingLoadPageResult} */
      this.pendingLoadPageResult_ = {netError: 0};


      // We listen to all requests made to fetch the main frame, but note that
      // we end up blocking requests to URLs that don't start with the expected
      // server prefix (see onBeforeSendHeaders_).
      // This is done because we will only know the prefix when we start to
      // load the page, but we add listeners at mojo setup time.
      const requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};

      this.webviewListener_ = this.webviewFinishedLoading_.bind(this);
      this.webview_.request.onBeforeSendHeaders.addListener(
          this.onBeforeSendHeaders_.bind(this), requestFilter,
          ['blocking', 'requestHeaders']);
      this.webview_.request.onHeadersReceived.addListener(
          this.onHeadersReceived_.bind(this), requestFilter,
          ['responseHeaders', 'extraHeaders']);
    }

    /**
     * @param {!chromeos.supervision.mojom.OnboardingPage} page
     * @return {Promise<{
     *     result: !chromeos.supervision.mojom.OnboardingLoadPageResult,
     *  }>}
     */
    loadPage(page) {
      // TODO(958995): Handle the case where we are still loading the previous
      // page but the controller wants to load the next one. For now we just
      // resolve the previous callback.
      if (this.pendingLoadPageCallback_) {
        this.pendingLoadPageCallback_({result: {netError: 0}});
      }

      this.page_ = page;
      this.pendingLoadPageCallback_ = null;
      this.pendingLoadPageResult_ = {netError: 0};

      this.webview_.addEventListener('loadstop', this.webviewListener_);
      this.webview_.addEventListener('loadabort', this.webviewListener_);
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
        this.pendingLoadPageResult_.customHeaderValue = header.value;
      }

      return {};
    }

    /**
     * Called when the webview sends a loadstop or loadabort event.
     * @param {!Event} e
     * @private
     */
    webviewFinishedLoading_(e) {
      this.webview_.removeEventListener('loadstop', this.webviewListener_);
      this.webview_.removeEventListener('loadabort', this.webviewListener_);

      if (e.type == 'loadabort') {
        this.pendingLoadPageResult_.netError = e.code;
      }

      this.pendingLoadPageCallback_({result: this.pendingLoadPageResult_});
    }
  }

  Polymer({
    is: 'supervision-onboarding',

    behaviors: [LoginScreenBehavior, OobeDialogHostBehavior],

    properties: {
      /** True if the webview loaded the page. */
      isReady_: {type: Boolean, value: false},

      hideBackButton_: {type: Boolean, value: true},
      hideSkipButton_: {type: Boolean, value: true},
      hideNextButton_: {type: Boolean, value: true},
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
        commonScreenSize: true,
        resetAllowed: true,
      });
    },

    /**
     * @param {!chromeos.supervision.mojom.OnboardingPresentation} presentation
     * @private
     */
    setPresentation_: function(presentation) {
      this.isReady_ = presentation.state ==
          chromeos.supervision.mojom.OnboardingPresentationState.kReady;

      this.hideBackButton_ = !presentation.canShowPreviousPage;
      this.hideSkipButton_ = !presentation.canSkipFlow;
      this.hideNextButton_ = !presentation.canShowNextPage;
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
  });
}

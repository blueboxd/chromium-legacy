// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe ARC Terms of Service screen implementation.
 */

login.createScreen('ArcTermsOfServiceScreen', 'arc-tos', function() {
  return {
    EXTERNAL_API: [
      'setMetricsMode', 'setBackupAndRestoreMode', 'setLocationServicesMode',
      'loadPlayStoreToS', 'setArcManaged', 'setupForDemoMode', 'clearDemoMode',
      'setTosForTesting'
    ],

    /** @override */
    decorate(element) {
      this.countryCode_ = null;
      this.language_ = null;
      this.pageReady_ = false;

      /* The hostname of the url where the terms of service will be fetched.
       * Overwritten by tests to load terms of service from local test server.*/
      this.termsOfServiceHostName_ = 'https://play.google.com';
      if (loadTimeData.valueExists('arcTosHostNameForTesting')) {
        this.setTosHostNameForTesting_(
            loadTimeData.getString('arcTosHostNameForTesting'));
      }
    },

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.ONBOARDING;
    },

    /**
     * Returns current language that can be updated in OOBE flow. If OOBE flow
     * does not exist then use navigator.language.
     *
     * @private
     */
    getCurrentLanguage_() {
      const LANGUAGE_LIST_ID = 'languageList';
      if (loadTimeData.valueExists(LANGUAGE_LIST_ID)) {
        var languageList = loadTimeData.getValue(LANGUAGE_LIST_ID);
        if (languageList) {
          var language = getSelectedValue(languageList);
          if (language) {
            return language;
          }
        }
      }
      return navigator.language;
    },

    /**
     * Makes sure that UI is initialized.
     *
     * @private
     */
    ensureInitialized_() {
      if (this.pageReady_) {
        return;
      }

      this.pageReady_ = true;
      $('arc-tos-root').screen = this;
      $('arc-tos-root').setupOverlay();

      var termsView = this.getElement_('arcTosView');
      var requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};

      termsView.request.onErrorOccurred.addListener(
          this.onTermsViewErrorOccurred.bind(this), requestFilter);
      termsView.addEventListener(
          'contentload', this.onTermsViewContentLoad.bind(this));

      // Open links from webview in overlay dialog.
      var self = this;
      termsView.addEventListener('newwindow', function(event) {
        event.preventDefault();
        self.showUrlOverlay(event.targetUrl);
      });

      termsView.addContentScripts([{
        name: 'postProcess',
        matches: [this.getTermsOfServiceHostNameForMatchPattern_() + '/*'],
        css: {files: ['playstore.css']},
        js: {files: ['playstore.js']},
        run_at: 'document_end'
      }]);

      this.getElement_('arcPolicyLink').onclick = function() {
        termsView.executeScript(
            {code: 'getPrivacyPolicyLink();'}, function(results) {
              if (results && results.length == 1 &&
                  typeof results[0] == 'string') {
                self.showUrlOverlay(results[0]);
              } else {
                var defaultLink = 'https://www.google.com/intl/' +
                    self.getCurrentLanguage_() + '/policies/privacy/';
                self.showUrlOverlay(defaultLink);
              }
            });
      };

      var overlayUrl = this.getElement_('arcTosOverlayWebview');
      overlayUrl.addContentScripts([{
        name: 'postProcess',
        matches: ['https://support.google.com/*'],
        css: {files: ['overlay.css']},
        run_at: 'document_end'
      }]);

      // Update the screen size after setup layout.
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Sets current metrics mode.
     * @param {string} text Describes current metrics state.
     * @param {boolean} visible If metrics text is visible.
     */
    setMetricsMode(text, visible) {
      $('arc-tos-root').isMetricsHidden = !visible;
      $('arc-tos-root').metricsText = text;
    },

    /**
     * Sets current backup and restore mode.
     * @param {boolean} enabled Defines the value for backup and restore
     *                          checkbox.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setBackupAndRestoreMode(enabled, managed) {
      $('arc-tos-root').backupRestore = enabled;
      $('arc-tos-root').backupRestoreManaged = managed;
    },

    /**
     * Sets current usage of location service opt in mode.
     * @param {boolean} enabled Defines the value for location service opt in.
     * @param {boolean} managed Defines whether this setting is set by policy.
     */
    setLocationServicesMode(enabled, managed) {
      $('arc-tos-root').backupRestore = enabled;
      $('arc-tos-root').backupRestoreManaged = managed;
    },

    /**
     * Loads Play Store ToS in case country code has been changed or previous
     * attempt failed.
     * @param {string} countryCode Country code based on current timezone.
     */
    loadPlayStoreToS(countryCode) {
      // Make sure page is initialized for login mode. For OOBE mode, page is
      // initialized as result of handling updateLocalizedContent.
      this.ensureInitialized_();

      var language = this.getCurrentLanguage_();
      countryCode = countryCode.toLowerCase();

      if (this.language_ && this.language_ == language && this.countryCode_ &&
          this.countryCode_ == countryCode &&
          !this.classList.contains('error') && !this.usingOfflineTerms_ &&
          this.tosContent_) {
        this.enableButtons_(true);
        return;
      }

      // Store current ToS parameters.
      this.language_ = language;
      this.countryCode_ = countryCode;

      var scriptSetParameters =
          'document.countryCode = \'' + countryCode + '\';';
      scriptSetParameters += 'document.language = \'' + language + '\';';
      scriptSetParameters += 'document.viewMode = \'large-view\';';

      var termsView = this.getElement_('arcTosView');

      termsView.removeContentScripts(['preProcess']);
      termsView.addContentScripts([{
        name: 'preProcess',
        matches: [this.getTermsOfServiceHostNameForMatchPattern_() + '/*'],
        js: {code: scriptSetParameters},
        run_at: 'document_start'
      }]);

      // Try to use currently loaded document first.
      var self = this;
      if (termsView.src != '' && this.classList.contains('arc-tos-loaded')) {
        var navigateScript = 'processLangZoneTerms(true, \'' + language +
            '\', \'' + countryCode + '\');';
        termsView.executeScript({code: navigateScript}, function(results) {
          if (!results || results.length != 1 ||
              typeof results[0] !== 'boolean' || !results[0]) {
            self.reloadPlayStoreToS();
          }
        });
      } else {
        this.reloadPlayStoreToS();
      }
    },

    /**
     * Sets Play Store terms of service for testing.
     * @param {string} terms Fake Play Store terms of service.
     */
    setTosForTesting(terms) {
      this.tosContent_ = terms;
      this.usingOfflineTerms_ = true;
      this.setTermsViewContentLoadedState_();
    },

    /**
     * Sets Play Store hostname url used to fetch terms of service for testing.
     * @param {string} hostname hostname used to fetch terms of service.
     */
    setTosHostNameForTesting_(hostname) {
      this.termsOfServiceHostName_ = hostname;
      this.reloadsLeftForTesting_ = 1;

      // Enable loading content script 'playstore.js' when fetching ToS from
      // the test server.
      var termsView = this.getElement_('arcTosView');
      termsView.removeContentScripts(['postProcess']);
      termsView.addContentScripts([{
        name: 'postProcess',
        matches: [this.getTermsOfServiceHostNameForMatchPattern_() + '/*'],
        css: {files: ['playstore.css']},
        js: {files: ['playstore.js']},
        run_at: 'document_end'
      }]);
    },

    /**
     * Sets if Arc is managed. ToS webview should not be visible if Arc is
     * manged.
     * @param {boolean} managed Defines whether this setting is set by policy.
     * @param {boolean} whether current account is a child account.
     */
    setArcManaged(managed, child) {
      var visibility = managed ? 'hidden' : 'visible';
      this.getElement_('arcTosViewContainer').style.visibility = visibility;
      $('arc-tos-root').isChild = child;
    },

    /**
     * Handles Accept button click.
     */
    onAccept() {
      this.enableButtons_(false);

      var isBackupRestoreEnabled = $('arc-tos-root').backupRestore;
      var isLocationServiceEnabled = $('arc-tos-root').locationService;
      var reviewArcSettings = $('arc-tos-root').reviewSettings;
      chrome.send('arcTermsOfServiceAccept', [
        isBackupRestoreEnabled, isLocationServiceEnabled, reviewArcSettings,
        this.tosContent_
      ]);
    },

    /**
     * Enables/Disables set of buttons: Accept, Skip, Retry.
     * @param {boolean} enable Buttons are enabled if set to true.
     *
     * @private
     */
    enableButtons_(enable) {
      $('arc-tos-root').arcTosButtonsDisabled = !enable;
    },

    /**
     * Opens external URL in popup overlay.
     * @param {string} targetUrl URL to open.
     */
    showUrlOverlay(targetUrl) {
      if (this.usingOfflineTerms_) {
        const TERMS_URL = 'chrome://terms/arc/privacy_policy';
        WebViewHelper.loadUrlContentToWebView(
            this.getElement_('arcTosOverlayWebview'), TERMS_URL,
            WebViewHelper.ContentType.PDF);
      }
      $('arc-tos-root').showUrlOverlay(targetUrl, this.usingOfflineTerms_);
    },

    /**
     * Reloads Play Store ToS.
     */
    reloadPlayStoreToS() {
      if (this.reloadsLeftForTesting_ !== undefined) {
        if (this.reloadsLeftForTesting_ <= 0)
          return;
        --this.reloadsLeftForTesting_;
      }
      this.termsError = false;
      this.usingOfflineTerms_ = false;
      var termsView = this.getElement_('arcTosView');
      termsView.src = this.termsOfServiceHostName_ + '/about/play-terms.html';
      this.removeClass_('arc-tos-loaded');
      this.removeClass_('error');
      this.addClass_('arc-tos-loading');
      this.enableButtons_(false);
    },

    /**
     * Sets up the variant of the screen dedicated falsedemo mode.
     */
    setupForDemoMode() {
      $('arc-tos-root').demoMode = true;
    },

    /**
     * Sets up the variant of the screen dedicated for demo mode.
     */
    clearDemoMode() {
      $('arc-tos-root').demoMode = false;
    },

    /**
     * Adds new class to the list of classes of root OOBE style.
     * @param {string} className class to remove.
     *
     * @private
     */
    addClass_(className) {
      $('arc-tos-root').getElement('arcTosDialog').classList.add(className);
    },

    /**
     * Removes class from the list of classes of root OOBE style.
     * @param {string} className class to remove.
     *
     * @private
     */
    removeClass_(className) {
      $('arc-tos-root').getElement('arcTosDialog').classList.remove(className);
    },

    /**
     * Checks if class exsists in the list of classes of root OOBE style.
     * @param {string} className class to check.
     *
     * @private
     */
    hasClass_(className) {
      return $('arc-tos-root')
          .getElement('arcTosDialog')
          .classList.contains(className);
    },

    /**
     * Returns a match pattern compatible version of termsOfServiceHostName_ by
     * stripping the port number part of the hostname. During tests
     * termsOfServiceHostName_ will contain a port number part.
     * @return {string}
     * @private
     */
    getTermsOfServiceHostNameForMatchPattern_() {
      return this.termsOfServiceHostName_.replace(/:[0-9]+/, '');
    },

    /**
     * Handles event when terms view is loaded.
     */
    onTermsViewContentLoad() {
      if (this.termsError) {
        return;
      }

      var termsView = this.getElement_('arcTosView');
      if (this.usingOfflineTerms_) {
        // Process offline ToS. Scripts added to web view by addContentScripts()
        // are not executed when using data url.
        this.tosContent_ = termsView.src;
        var setParameters =
            `document.body.classList.add('large-view', 'offline-terms');`;
        termsView.executeScript({code: setParameters});
        termsView.insertCSS({file: 'playstore.css'});
        this.setTermsViewContentLoadedState_();
      } else {
        // Process online ToS.
        var getToSContent = {code: 'getToSContent();'};
        termsView.executeScript(
            getToSContent, this.onGetToSContent_.bind(this));
      }
    },

    /**
     * Handles callback for getToSContent.
     */
    onGetToSContent_(results) {
      if (!results || results.length != 1 || typeof results[0] !== 'string') {
        this.showError_();
        return;
      }

      this.tosContent_ = results[0];
      this.setTermsViewContentLoadedState_();
    },

    /**
     * Sets the screen in the loaded state. Should be called after arc terms
     * were loaded.
     * @private
     */
    setTermsViewContentLoadedState_() {
      this.removeClass_('arc-tos-loading');
      this.removeClass_('error');
      this.addClass_('arc-tos-loaded');

      this.enableButtons_(true);
      $('arc-tos-root').showFullDialog = false;
      this.getElement_('arcTosNextButton').focus();
    },

    /**
     * Handles event when terms view cannot be loaded.
     */
    onTermsViewErrorOccurred(details) {
      // If in demo mode fallback to offline Terms of Service copy.
      if (this.isDemoModeSetup_()) {
        this.usingOfflineTerms_ = true;
        const TERMS_URL = 'chrome://terms/arc/terms';
        var webView = this.getElement_('arcTosView');
        WebViewHelper.loadUrlContentToWebView(
            webView, TERMS_URL, WebViewHelper.ContentType.HTML);
        return;
      }
      this.showError_();
    },

    /**
     * Shows error UI when terms view cannot be loaded or terms content cannot
     * be fetched from webview.
     */
    showError_() {
      this.termsError = true;
      this.removeClass_('arc-tos-loading');
      this.removeClass_('arc-tos-loaded');
      this.addClass_('error');

      this.enableButtons_(true);
      this.getElement_('arcTosRetryButton').focus();
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow(data) {
      this.focusButton_();

      $('arc-tos-root').onBeforeShow();

      var isDemoModeSetup = this.isDemoModeSetup_();
      if (isDemoModeSetup) {
        this.setMetricsMode('arcTextMetricsManagedEnabled', true);
      }
      $('arc-tos-root').accpetTextKey = isDemoModeSetup ?
          'arcTermsOfServiceAcceptAndContinueButton' :
          'arcTermsOfServiceAcceptButton';
      $('arc-tos-root').googleServiceConfirmationText = isDemoModeSetup ?
          'arcAcceptAndContinueGoogleServiceConfirmation' :
          'arcTextGoogleServiceConfirmation';
    },

    /** @override */
    onBeforeHide() {
      this.reset_();
    },

    /**
     * Resets UI elements to their initial state.
     * @private
     */
    reset_() {
      $('arc-tos-root').showFullDialog = false;
      this.getElement_('arcTosNextButton').focus();
    },

    /**
     * Ensures the correct button is focused when the page is shown.
     *
     * @private
     */
    focusButton_() {
      var id;
      if (this.hasClass_('arc-tos-loaded')) {
        id = 'arcTosNextButton';
      } else if (this.hasClass_('error')) {
        id = 'arcTosRetryButton';
      }

      if (typeof id === 'undefined')
        return;

      setTimeout(function() {
        this.getElement_(id).focus();
      }.bind(this), 0);
    },

    /**
     * Returns requested element from related part of HTML.
     * @param {string} id Id of an element to find.
     *
     * @private
     */
    getElement_(id) {
      return $('arc-tos-root').getElement(id);
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent() {
      this.ensureInitialized_();

      // We might need to reload Play Store ToS in case language was changed.
      if (this.countryCode_) {
        this.loadPlayStoreToS(this.countryCode_);
      }
    },

    /**
     * Returns whether arc terms are shown as a part of demo mode setup.
     * @return {boolean}
     * @private
     */
    isDemoModeSetup_() {
      return $('arc-tos-root').demoMode;
    }
  };
});

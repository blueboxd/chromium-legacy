// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Enterprise Enrollment screen.
 */

/* Code which is embedded inside of the webview. See below for details.
/** @const */
var INJECTED_WEBVIEW_SCRIPT = String.raw`
                    (function() {
                       // <include src="../keyboard/keyboard_utils.js">
                       keyboard.initializeKeyboardFlow(true);
                     })();`;

/** @const */ var ENROLLMENT_STEP = {
  SIGNIN: 'signin',
  AD_JOIN: 'ad-join',
  WORKING: 'working',
  ATTRIBUTE_PROMPT: 'attribute-prompt',
  ERROR: 'error',
  SUCCESS: 'success',

  /* TODO(dzhioev): define this step on C++ side.
   */
  ATTRIBUTE_PROMPT_ERROR: 'attribute-prompt-error',
  ACTIVE_DIRECTORY_JOIN_ERROR: 'active-directory-join-error',
};

Polymer({
  is: 'enterprise-enrollment',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Reference to OOBE screen object.
     * @type {!{
     *     onAuthFrameLoaded_: function(),
     *     onAuthCompleted_: function(string),
     *     onAdCompleteLogin_: function(string, string, string, string, string),
     *     onAdUnlockConfiguration_: function(string),
     *     closeEnrollment_: function(string),
     *     onAttributesEntered_: function(string, string),
     * }}
     */
    screen: {
      type: Object,
    },

    /**
     * The current step. This is the last value passed to showStep().
     */
    currentStep_: {
      type: String,
      value: '',
    },

    /**
     * Indicates if authenticator have shown internal dialog.
     */
    authenticatorDialogDisplayed_: {
      type: Boolean,
      value: false,
    },

    /**
     * Domain the device was enrolled to.
     */
    enrolledDomain_: {
      type: String,
      value: '',
    },

    /**
     * Name of the device that was enrolled.
     */
    deviceName_: {
      type: String,
      value: 'Chromebook',
    },

    /**
     * Text on the error screen.
     */
    errorText_: {
      type: String,
      value: '',
    },

    /**
     * Controls if there will be "Try Again" button on the error screen.
     *
     * True:  Error Nature Recoverable
     * False: Error Nature Fatal
     */
    canRetryAfterError_: {
      type: Boolean,
      value: true,
    },

    /**
     * Device attribute : Asset ID.
     */
    assetId_: {
      type: String,
      value: '',
    },

    /**
     * Device attribute : Location.
     */
    deviceLocation_: {
      type: String,
      value: '',
    },

    /**
     * Whether the enrollment is automatic
     *
     * True:  Automatic (Attestation-based)
     * False: Manual (OAuth)
     */
    isAutoEnroll_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the enrollment is enforced and cannot be skipped.
     *
     * True:  Enrollment Enforced
     * False: Enrollment Optional
     */
    isForced_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the SAML SSO page is visible.
     * @private
     */
    isSamlSsoVisible_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Authenticator object that wraps GAIA webview.
   */
  authenticator_: null,

  /**
   * We block esc, back button and cancel button until gaia is loaded to
   * prevent multiple cancel events.
   */
  isCancelDisabled_: null,

  get isCancelDisabled() {
    return this.isCancelDisabled_;
  },
  set isCancelDisabled(disabled) {
    this.isCancelDisabled_ = disabled;
  },

  isManualEnrollment_: undefined,

  /**
   * An element containing navigation buttons.
   */
  navigation_: undefined,

  /**
   * An element containing UI to join an AD domain.
   * @type {OfflineAdLoginElement}
   * @private
   */
  offlineAdUi_: undefined,

  /**
   * Value contained in the last received 'backButton' event.
   * @type {boolean}
   * @private
   */
  lastBackMessageValue_: false,

  ready() {
    this.navigation_ = this.$['oauth-enroll-navigation'];
    this.offlineAdUi_ = this.$['oauth-enroll-ad-join-ui'];

    let authView = this.$['oauth-enroll-auth-view'];
    this.authenticator_ = new cr.login.Authenticator(authView);

    // Establish an initial messaging between content script and
    // host script so that content script can message back.
    authView.addEventListener('loadstop', function(e) {
      e.target.contentWindow.postMessage(
          'initialMessage', authView.src);
    });

    // When we get the advancing focus command message from injected content
    // script, we can execute it on host script context.
    window.addEventListener('message', function(e) {
      if (e.data == 'forwardFocus')
        keyboard.onAdvanceFocus(false);
      else if (e.data == 'backwardFocus')
        keyboard.onAdvanceFocus(true);
    });

    this.authenticator_.addEventListener(
        'ready', (function() {
                   if (this.currentStep_ != ENROLLMENT_STEP.SIGNIN)
                     return;
                   this.isCancelDisabled = false;
                   this.screen.onAuthFrameLoaded_();
                 }).bind(this));

    this.authenticator_.addEventListener(
        'authCompleted',
        (function(e) {
          var detail = e.detail;
          if (!detail.email) {
            this.showError(
                loadTimeData.getString('fatalEnrollmentError'), false);
            return;
          }
          this.screen.onAuthCompleted_(detail.email);
        }).bind(this));

    this.offlineAdUi_.addEventListener('authCompleted', function(e) {
      this.offlineAdUi_.disabled = true;
      this.offlineAdUi_.loading = true;
      this.screen.onAdCompleteLogin_(
        e.detail.machine_name,
        e.detail.distinguished_name,
        e.detail.encryption_types,
        e.detail.username,
        e.detail.password);
    }.bind(this));
    this.offlineAdUi_.addEventListener('unlockPasswordEntered', function(e) {
      this.offlineAdUi_.disabled = true;
      this.screen.onAdUnlockConfiguration_(e.detail.unlock_password);
    }.bind(this));

    this.authenticator_.addEventListener(
        'authFlowChange', (function(e) {
                            var isSAML = this.authenticator_.authFlow ==
                                cr.login.Authenticator.AuthFlow.SAML;
                            if (isSAML) {
                              this.$['oauth-saml-notice-message'].textContent =
                                  loadTimeData.getStringF(
                                      'samlNotice',
                                      this.authenticator_.authDomain);
                            }
                            this.isSamlSsoVisible_ = isSAML;
                            if (Oobe.getInstance().currentScreen == this)
                              Oobe.getInstance().updateScreenSize(this);
                            this.lastBackMessageValue_ = false;
                            this.updateControlsState();
                          }).bind(this));

    this.authenticator_.addEventListener(
        'backButton', (function(e) {
                        this.lastBackMessageValue_ = !!e.detail;
                        this.$['oauth-enroll-auth-view'].focus();
                        this.updateControlsState();
                      }).bind(this));

    this.authenticator_.addEventListener(
        'dialogShown', (function(e) {
                         this.authenticatorDialogDisplayed_ = true;
                       }).bind(this));

    this.authenticator_.addEventListener(
        'dialogHidden', (function(e) {
                          this.authenticatorDialogDisplayed_ = false;
                        }).bind(this));

    this.authenticator_.insecureContentBlockedCallback =
        (function(url) {
          this.showError(
              loadTimeData.getStringF('insecureURLEnrollmentError', url),
              false);
        }).bind(this);

    this.authenticator_.missingGaiaInfoCallback =
        (function() {
          this.showError(
              loadTimeData.getString('fatalEnrollmentError'), false);
        }).bind(this);

    this.$['oauth-enroll-learn-more-link']
        .addEventListener('click', function(event) {
          chrome.send('oauthEnrollOnLearnMore');
        });
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload, contains the signin frame
   * URL.
   */
  onBeforeShow(data) {
    if (Oobe.getInstance().forceKeyboardFlow) {
      // We run the tab remapping logic inside of the webview so that the
      // simulated tab events will use the webview tab-stops. Simulated tab
      // events created from the webui treat the entire webview as one tab
      // stop. Real tab events do not do this. See crbug.com/543865.
      this.$['oauth-enroll-auth-view'].addContentScripts([{
        name: 'injectedTabHandler',
        matches: ['http://*/*', 'https://*/*'],
        js: {code: INJECTED_WEBVIEW_SCRIPT},
        run_at: 'document_start'
      }]);
    }

    this.authenticator_.setWebviewPartition(data.webviewPartitionName);

    this.isSamlSsoVisible_ = false;

    var gaiaParams = {};
    gaiaParams.gaiaUrl = data.gaiaUrl;
    gaiaParams.clientId = data.clientId;
    gaiaParams.needPassword = false;
    gaiaParams.hl = data.hl;
    if (data.management_domain) {
      gaiaParams.enterpriseEnrollmentDomain = data.management_domain;
      gaiaParams.emailDomain = data.management_domain;
    }
    gaiaParams.flow = data.flow;
    this.authenticator_.load(
        cr.login.Authenticator.AuthMode.DEFAULT, gaiaParams);

    this.isManualEnrollment_ = data.enrollment_mode === 'manual';
    this.isForced_ = data.is_enrollment_enforced;
    this.isAutoEnroll_ = data.attestationBased;

    this.authenticatorDialogDisplayed_ = false;

    this.offlineAdUi_.onBeforeShow();
    if (!this.currentStep_) {
      this.showStep(data.attestationBased ?
          ENROLLMENT_STEP.WORKING : ENROLLMENT_STEP.SIGNIN);
    }
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent: function() {
    this.offlineAdUi_.i18nUpdateLocale();
    this.i18nUpdateLocale();
  },

  /**
   * Shows attribute-prompt step with pre-filled asset ID and
   * location.
   */
  showAttributePromptStep(annotatedAssetId, annotatedLocation) {
    this.assetId_ = annotatedAssetId;
    this.deviceLocation_ = annotatedLocation;
    this.showStep(ENROLLMENT_STEP.ATTRIBUTE_PROMPT);
  },


  /**
   * Sets the type of the device and the enterprise domain to be shown.
   *
   * @param {string} enterprise_domain
   * @param {string} device_type
   */
  setEnterpriseDomainAndDeviceType(enterprise_domain, device_type) {
    this.enrolledDomain_ = enterprise_domain;
    this.deviceName_ = device_type;
  },

  /**
   * Cancels the current authentication and drops the user back to the next
   * screen (either the next authentication or the login screen).
   */
  cancel() {
    if (this.isCancelDisabled)
      return;
    this.isCancelDisabled = true;
    this.screen.closeEnrollment_('cancel');
  },

  /**
   * Switches between the different steps in the enrollment flow.
   * @param {string} step the steps to show, one of "signin", "working",
   * "attribute-prompt", "error", "success".
   */
  showStep(step) {
    this.isCancelDisabled =
        (step == ENROLLMENT_STEP.SIGNIN && !this.isManualEnrollment_) ||
        step == ENROLLMENT_STEP.AD_JOIN || step == ENROLLMENT_STEP.WORKING;

    this.currentStep_ = step;

    if (this.isErrorStep_(step)) {
      this.$['oauth-enroll-error-card'].show();
    } else if (step == ENROLLMENT_STEP.SIGNIN) {
      this.$['oauth-enroll-auth-view'].focus();
    } else if (step == ENROLLMENT_STEP.SUCCESS) {
      this.$['oauth-enroll-success-card'].show();
    } else if (step == ENROLLMENT_STEP.ATTRIBUTE_PROMPT) {
      this.$['oauth-enroll-attribute-prompt-card'].show();
    } else if (step == ENROLLMENT_STEP.AD_JOIN) {
      this.offlineAdUi_.disabled = false;
      this.offlineAdUi_.loading = false;
      this.offlineAdUi_.focus();
    }

    this.lastBackMessageValue_ = false;
    this.updateControlsState();
  },

  doReload() {
    this.lastBackMessageValue_ = false;
    this.authenticatorDialogDisplayed_ = false;
    this.authenticator_.reload();
    this.updateControlsState();
  },

  /**
   * Sets Active Directory join screen params.
   * @param {string} machineName
   * @param {string} userName
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
   * @param {boolean} showUnlockConfig true if there is an encrypted
   * configuration (and not unlocked yet).
   */
  setAdJoinParams(machineName, userName, errorState, showUnlockConfig) {
    this.offlineAdUi_.disabled = false;
    this.offlineAdUi_.machineName = machineName;
    this.offlineAdUi_.userName = userName;
    this.offlineAdUi_.errorState = errorState;
    this.offlineAdUi_.unlockPasswordStep = showUnlockConfig;
  },

  /**
   * Sets Active Directory join screen with the unlocked configuration.
   * @param {Array<JoinConfigType>} options
   */
  setAdJoinConfiguration(options) {
    this.offlineAdUi_.disabled = false;
    this.offlineAdUi_.setJoinConfigurationOptions(options);
    this.offlineAdUi_.unlockPasswordStep = false;
  },

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  skipAttributes_() {
    this.showStep(ENROLLMENT_STEP.SUCCESS);
  },

  /**
   * Uploads the device attributes to server. This goes to C++ side through
   * |chrome| and launches the device attribute update negotiation.
   */
  submitAttributes_() {
    this.screen.onAttributesEntered_(this.assetId_, this.deviceLocation_);
  },

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  onBackButtonClicked_() {
    if (this.currentStep_ == ENROLLMENT_STEP.SIGNIN) {
      if (this.lastBackMessageValue_) {
        this.lastBackMessageValue_ = false;
        this.$['oauth-enroll-auth-view'].back();
      } else {
        this.cancel();
      }
    }
  },

  /**
   * Returns true if we are at the begging of enrollment flow (i.e. the email
   * page).
   *
   * @type {boolean}
   */
  isAtTheBeginning() {
    return !this.lastBackMessageValue_ &&
        this.currentStep_ == ENROLLMENT_STEP.SIGNIN;
  },

  /**
   * Updates visibility of navigation buttons.
   */
  updateControlsState() {
    this.navigation_.refreshVisible = this.isAtTheBeginning() &&
        this.isManualEnrollment_ === false;
  },

  /**
   * Notifies chrome that enrollment have finished.
   */
  onEnrollmentFinished_() {
    this.screen.closeEnrollment_('done');
  },

  /**
   * Generates message on the success screen.
   */
  successText_(locale, device, domain) {
    return this.i18nAdvanced(
        'oauthEnrollAbeSuccessDomain', {substitutions: [device, domain]});
  },

  isEmpty_(str) {
    return !str;
  },

  /**
   * Simple equality comparison function.
   */
  eq_(currentStep, expectedStep) {
    return currentStep == expectedStep;
  },

  /**
   * ERROR DIALOG LOGIC:
   *
   *    The error displayed on the enrollment error dialog depends on the nature
   *    of the error (_recoverable_/_fatal_), on the authentication mechanism
   *    (_manual_/_automatic_), and on whether the enrollment is _enforced_ or
   *    _optional_.
   *
   *    AUTH MECH |  ENROLLMENT |  ERROR NATURE            Buttons Layout
   *    ----------------------------------------
   *    AUTOMATIC |   ENFORCED  |  RECOVERABLE    [    [Enroll Man.][Try Again]]
   *    AUTOMATIC |   ENFORCED  |  FATAL          [               [Enroll Man.]]
   *    AUTOMATIC |   OPTIONAL  |  RECOVERABLE    [    [Enroll Man.][Try Again]]
   *    AUTOMATIC |   OPTIONAL  |  FATAL          [               [Enroll Man.]]
   *
   *    MANUAL    |   ENFORCED  |  RECOVERABLE    [[Back]           [Try Again]]
   *    MANUAL    |   ENFORCED  |  FATAL          [[Back]                      ]
   *    MANUAL    |   OPTIONAL  |  RECOVERABLE    [           [Skip][Try Again]]
   *    MANUAL    |   OPTIONAL  |  FATAL          [                      [Skip]]
   *
   *    -  The buttons [Back], [Enroll Manually] and [Skip] all call 'cancel'.
   *    - [Enroll Manually] and [Skip] are the same button (GENERIC CANCEL) and
   *      are relabeled depending on the situation.
   *    - [Back] is only shown the button "GENERIC CANCEL" above isn't shown.
   */

  /**
   * Sets an error message and switches to the error screen.
   * @param {string} message the error message.
   * @param {boolean} retry whether the retry link should be shown.
   */
  showError: function(message, retry) {
    this.errorText_ = message;
    this.canRetryAfterError_ = retry;

    if (this.currentStep_ == ENROLLMENT_STEP.ATTRIBUTE_PROMPT) {
      this.showStep(ENROLLMENT_STEP.ATTRIBUTE_PROMPT_ERROR);
    } else if (this.currentStep_ == ENROLLMENT_STEP.AD_JOIN) {
      this.showStep(ENROLLMENT_STEP.ACTIVE_DIRECTORY_JOIN_ERROR);
    } else {
      this.showStep(ENROLLMENT_STEP.ERROR);
    }
  },

  /**
   * Simple equality comparison function.
   */
  isErrorStep_(currentStep) {
    return currentStep == ENROLLMENT_STEP.ERROR ||
           currentStep == ENROLLMENT_STEP.ATTRIBUTE_PROMPT_ERROR ||
           currentStep == ENROLLMENT_STEP.ACTIVE_DIRECTORY_JOIN_ERROR;
  },

  /**
   *  Provides the label for the generic cancel button (Skip / Enroll Manually)
   *
   *  During automatic enrollment, the label is 'Enroll Manually'.
   *  During manual enrollment, the label is 'Skip'.
   * @private
   */
  getCancelButtonLabel_(locale_, is_automatic) {
    if (this.isAutoEnroll_) {
      return 'oauthEnrollManualEnrollment';
    } else {
      return 'oauthEnrollSkip';
    }
  },

  /**
   *  Whether the "GENERIC CANCEL" (SKIP / ENROLL_MANUALLY ) button should be
   *  shown. It is only shown when in 'AUTOMATIC' mode OR when in
   *  manual mode without enrollment enforcement.
   *
   *  When the enrollment is manual AND forced, a 'BACK' button will be shown.
   * @param {Boolean} automatic - Whether the enrollment is automatic
   * @param {Boolean} enforced  - Whether the enrollment is enforced
   * @private
   */
  isGenericCancel_(automatic, enforced) {
    return automatic || (!automatic && !enforced);
  },

  /**
   * Retries the enrollment process after an error occurred in a previous
   * attempt. This goes to the C++ side through |chrome| first to clean up the
   * profile, so that the next attempt is performed with a clean state.
   */
  doRetry_() {
    chrome.send('oauthEnrollRetry');
  },

  /**
   *  Event handler for the 'Try again' button that is shown upon an error
   *  during ActiveDirectory join.
   */
  onAdJoinErrorRetry_() {
    this.showStep(ENROLLMENT_STEP.AD_JOIN);
  },

  /**
   * Whether to show popup overlay under the dialog
   * @param {boolean} authenticatorDialogDisplayed
   * @param {boolean} isSamlSsoVisible
   * @return {boolean} True iff overlay popup should be displayed
   * @private
   */
  showPopupOverlay_(authenticatorDialogDisplayed, isSamlSsoVisible) {
    return authenticatorDialogDisplayed || isSamlSsoVisible;
  },
});

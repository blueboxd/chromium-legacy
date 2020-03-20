// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides locale output services for ChromeVox, which
 * uses language information to automatically change the TTS voice.
 * Please note: we use the term 'locale' to refer to language codes e.g.
 * 'en-US'. For more information on locales:
 * https://en.wikipedia.org/wiki/Locale_(computer_software)
 */

goog.provide('LocaleOutputHelper');

goog.require('StringUtil');

LocaleOutputHelper = class {
  /** @private */
  constructor() {
    /**
     * @const
     * @private {string}
     */
    LocaleOutputHelper.BROWSER_UI_LOCALE_ =
        chrome.i18n.getUILanguage().toLowerCase();
    /** @private {string} */
    this.currentLocale_ = LocaleOutputHelper.BROWSER_UI_LOCALE_ || '';
    /**
     * Confidence threshold to meet before assigning sub-node language.
     * @const
     * @private {number}
     */
    LocaleOutputHelper.PROBABILITY_THRESHOLD_ = 0.9;
    /**
     * Set to false as default, since sub-node language detection is still
     * experimental.
     * @private {boolean}
     */
    this.subNodeSwitchingEnabled_ = false;
    /** @private {!Array<!TtsVoice>} */
    this.availableVoices_ = [];
    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-chromevox-sub-node-language-' +
            'switching',
        (enabled) => {
          this.subNodeSwitchingEnabled_ = enabled;
        });
    const setAvailableVoices = () => {
      chrome.tts.getVoices((voices) => {
        this.availableVoices_ = voices || [];
      });
    };
    setAvailableVoices();
    if (window.speechSynthesis) {
      window.speechSynthesis.addEventListener(
          'voiceschanged', setAvailableVoices, /* useCapture */ false);
    }
  }

  /**
   * Main entry point. Routes arguments to either |atNodeLevel_| or
   * |atSubNodeLevel_| depending on the kind of locale switching the user
   * has specified.
   * @param {AutomationNode} node
   * @param {string} stringAttribute The string attribute whose value we want
   * to output.
   * @param {function(string, string)} appendWithLocaleCallback
   */
  assignLocalesAndAppend(node, stringAttribute, appendWithLocaleCallback) {
    if (!node || !node[stringAttribute]) {
      return;
    }

    const text = node[stringAttribute];
    if (this.subNodeSwitchingEnabled_) {
      this.atSubNodeLevel_(
          text, node, stringAttribute, appendWithLocaleCallback);
    } else {
      this.atNodeLevel_(text, node, appendWithLocaleCallback);
    }
  }

  /**
   * Assigns one locale to the entirety of |text|.
   * @param {string} text The text we want to append.
   * @param {!AutomationNode} node
   * @param {function(string, string)} appendWithLocaleCallback
   * @private
   */
  atNodeLevel_(text, node, appendWithLocaleCallback) {
    this.processText_(text, node, appendWithLocaleCallback);
  }

  /**
   * Assigns one or more locales to disjoint spans of |text|.
   * @param {string} text The text we want to append.
   * @param {!AutomationNode} node
   * @param {function(string, string)} appendWithLocaleCallback
   * @private
   */
  atSubNodeLevel_(text, node, stringAttribute, appendWithLocaleCallback) {
    const languageAnnotation =
        node.languageAnnotationForStringAttribute(stringAttribute);
    if (!languageAnnotation || languageAnnotation.length === 0) {
      appendWithLocaleCallback(text, LocaleOutputHelper.BROWSER_UI_LOCALE_);
      return;
    }

    for (const langSpan of languageAnnotation) {
      this.processText_(text, node, appendWithLocaleCallback, langSpan);
    }
  }

  /**
   * This method does the following:
   * 1. Computes output locale.
   * 2. Computes |outputString|
   * 3. Calls |appendWithLocaleCallback|
   * @param {string} text
   * @param {!AutomationNode} node The AutomationNode that owns |text|. Used
   * for context.
   * @param {function(string, string)} appendWithLocaleCallback
   * @param {!{
   *    language: string,
   *    startIndex: number,
   *    endIndex: number,
   *    probability: number}=} opt_langSpan
   * @private
   */
  processText_(text, node, appendWithLocaleCallback, opt_langSpan) {
    let startIndex = 0;
    let endIndex = text.length;
    let subNodeLocale = '';
    let subNodeProbability = 0;
    if (opt_langSpan) {
      startIndex = opt_langSpan.startIndex;
      endIndex = opt_langSpan.endIndex;
      subNodeLocale = opt_langSpan.language.toLowerCase();
      subNodeProbability = opt_langSpan.probability;
    }

    const nodeLocale = node.detectedLanguage || node.language || '';
    const newLocale =
        this.computeNewLocale_(nodeLocale, subNodeLocale, subNodeProbability);
    let outputString =
        StringUtil.getUnicodeSubstring_(text, startIndex, endIndex);
    const shouldUpdate = this.shouldUpdateLocale_(newLocale);
    if (this.hasVoiceForLocale_(newLocale)) {
      this.setCurrentLocale_(newLocale);
      if (shouldUpdate) {
        // Prepend the human-readable locale to |outputString|.
        const displayLanguage =
            chrome.accessibilityPrivate.getDisplayNameForLocale(
                newLocale /* Locale to translate */,
                newLocale /* Target locale */);
        outputString =
            Msgs.getMsg('language_switch', [displayLanguage, outputString]);
      }
    } else {
      // Alert the user that no voice is available for |newLocale|.
      this.setCurrentLocale_(LocaleOutputHelper.BROWSER_UI_LOCALE_);
      const displayLanguage =
          chrome.accessibilityPrivate.getDisplayNameForLocale(
              newLocale /* Locale to translate */,
              LocaleOutputHelper.BROWSER_UI_LOCALE_ /* Target locale */);
      outputString =
          Msgs.getMsg('voice_unavailable_for_language', [displayLanguage]);
    }

    appendWithLocaleCallback(outputString, this.currentLocale_);
  }

  /**
   * Use the following priority rankings:
   * 1. |subNodeLocale|, if we are confident in its accuracy.
   * 2. |nodeLocale|.
   * 3. |BROWSER_UI_LOCALE_|.
   * @param {string} nodeLocale
   * @param {string} subNodeLocale
   * @param {number} subNodeProbability
   * @return {string}
   * @private
   */
  computeNewLocale_(nodeLocale, subNodeLocale, subNodeProbability) {
    if (subNodeProbability > LocaleOutputHelper.PROBABILITY_THRESHOLD_) {
      return subNodeLocale;
    }

    nodeLocale = nodeLocale.toLowerCase();
    if (LocaleOutputHelper.isValidLocale_(nodeLocale)) {
      return nodeLocale;
    }

    return LocaleOutputHelper.BROWSER_UI_LOCALE_;
  }

  // TODO(akihiroota): http://crbug.com/1061222
  /**
   * Only compares the language components of each locale.
   * Note: Locale validation is the responsibility of the caller.
   * Ex: 'fr-fr' and 'fr-ca' have the same language component, but different
   * country components. We would return false in the above case. Ex: 'fr-ca'
   * and 'en-ca' have different language components, but the same country
   * components. We would return true in the above case.
   * @param {string} newLocale
   * @return {boolean}
   * @private
   */
  shouldUpdateLocale_(newLocale) {
    const newComponents = newLocale.split('-');
    const currentComponents = this.currentLocale_.split('-');
    return newComponents[0] !== currentComponents[0];
  }

  /**
   * @param {string} targetLocale
   * @return {boolean}
   * @private
   */
  hasVoiceForLocale_(targetLocale) {
    const components = targetLocale.split('-');
    if (!components || components.length === 0) {
      return false;
    }

    const targetLanguage = components[0];
    for (const voice of this.availableVoices_) {
      const candidateLanguage = voice.lang.toLowerCase().split('-')[0];
      if (candidateLanguage === targetLanguage) {
        return true;
      }
    }

    return false;
  }

  /**
   * @param {string} locale
   * @private
   */
  setCurrentLocale_(locale) {
    if (LocaleOutputHelper.isValidLocale_(locale)) {
      this.currentLocale_ = locale;
    }
  }

  // =============== Static Methods ==============

  /**
   * Creates a singleton instance of LocaleOutputHelper.
   * @private
   */
  static init() {
    if (LocaleOutputHelper.instance_ !== undefined) {
      console.error(
          'LocaleOutputHelper is a singleton, can only call |init| once');
      return;
    }

    LocaleOutputHelper.instance_ = new LocaleOutputHelper();
  }

  /**
   * @return {!LocaleOutputHelper}
   */
  static get instance() {
    if (!LocaleOutputHelper.instance_) {
      LocaleOutputHelper.init();
    }
    return LocaleOutputHelper.instance_;
  }

  /**
   * @param {string} locale
   * @return {boolean}
   * @private
   */
  static isValidLocale_(locale) {
    return chrome.accessibilityPrivate.getDisplayNameForLocale(
               locale, locale) !== '';
  }
};

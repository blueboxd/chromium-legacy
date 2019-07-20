// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides language switching services for ChromeVox, which
 * uses language detection information to automatically change the ChromeVox
 * output language.
 */

goog.provide('LanguageSwitching');

/**
 * The current output language. Initialize to the language of the browser or
 * empty string if unavailable.
 * @private {string}
 */
LanguageSwitching.currentLanguage_ =
    chrome.i18n.getUILanguage().toLowerCase() || '';

/**
 * Confidence threshold to meet before assigning inner-node language.
 * @const
 * @type {number}
 */
LanguageSwitching.PROBABILITY_THRESHOLD = 0.9;

/*
 * Main language switching function.
 * Cut up string attribute value into multiple spans with different
 * languages. Ranges and associated language information are returned by the
 * languageAnnotationsForStringAttribute() function.
 * @param {AutomationNode} node
 * @param {string} stringAttribute The string attribute for which we want to
 * get a language annotation
 * @param {Function<outputString: string, newLanguage: string>}
 *     appendStringWithLanguage
 * A callback that appends outputString to the output buffer in newLanguage.
 */
LanguageSwitching.assignLanguagesForStringAttribute = function(
    node, stringAttribute, appendStringWithLanguage) {
  if (!node)
    return;
  var languageAnnotation =
      node.languageAnnotationForStringAttribute(stringAttribute);
  var stringAttributeValue = node[stringAttribute];
  // If no language annotation is found, append entire stringAttributeValue to
  // buffer and do not switch languages.
  // TODO(akihiroota): Decide if we simply want to return if
  // stringAttributeValue is null.
  if (!languageAnnotation || languageAnnotation.length === 0) {
    appendStringWithLanguage(
        stringAttributeValue || '', LanguageSwitching.currentLanguage_);
    return;
  }
  // Split output based on language annotation.
  // Each object in languageAnnotation contains a language, probability,
  // and start/end indices that define a substring.
  for (var i = 0; i < languageAnnotation.length; ++i) {
    var speechProps = new Output.SpeechProperties();
    var startIndex = languageAnnotation[i].startIndex;
    var endIndex = languageAnnotation[i].endIndex;
    var language = languageAnnotation[i].language;
    var probability = languageAnnotation[i].probability;

    var outputString = LanguageSwitching.buildOutputString(
        stringAttributeValue, startIndex, endIndex);
    var newLanguage =
        LanguageSwitching.decideNewLanguage(node, language, probability);
    if (LanguageSwitching.didLanguageSwitch(newLanguage)) {
      LanguageSwitching.currentLanguage_ = newLanguage;
      var displayLanguage =
          chrome.accessibilityPrivate.getDisplayLanguage(newLanguage);
      // Prepend the human-readable language to outputString if language
      // switched.
      outputString =
          Msgs.getMsg('language_switch', [displayLanguage, outputString]);
    }
    appendStringWithLanguage(newLanguage, outputString);
  }
};

/**
 * Run error checks on language data and decide new output language.
 * @param {!AutomationNode} node
 * @param {string} innerNodeLanguage
 * @param {number} probability
 * @return {string}
 */
LanguageSwitching.decideNewLanguage = function(
    node, innerNodeLanguage, probability) {
  // Use the following priority rankings when deciding language.
  // 1. Inner-node language. If we can detect inner-node language with a high
  // enough probability of accuracy, then we should use it.
  // 2. Node-level detected language.
  // 3. Author-provided language. This language is also assigned at the node
  // level.
  // 4. LanguageSwitching.currentLanguage_. If we do not have enough language
  // data, then we should not switch languages.

  // Use innerNodeLanguage if probability exceeds threshold.
  if (probability > LanguageSwitching.PROBABILITY_THRESHOLD)
    return innerNodeLanguage.toLowerCase();

  // Use detected language as nodeLevelLanguage, if present.
  // If no detected language, use author-provided language.
  var nodeLevelLanguage = node.detectedLanguage || node.language;
  // If nodeLevelLanguage is null, then do not switch languages.
  // We do not have enough information to make a confident language assignment,
  // so we will just stick with the current language.
  if (!nodeLevelLanguage)
    return LanguageSwitching.currentLanguage_;

  nodeLevelLanguage = nodeLevelLanguage.toLowerCase();

  // TODO(akihiroota): Move validation into separate function.
  // Validate nodeLevelLanguage, since there's the possibility that it comes
  // from the author.
  // There are five possible components of a language code. See link for more
  // details: http://userguide.icu-project.org/locale
  // The TTS Engine handles parsing language codes, but it needs to have a
  // valid language component for the engine not to crash.
  // For example, given the language code 'en-US', 'en' is the language
  // component.
  var langComponentArray = nodeLevelLanguage.split('-');
  if (!langComponentArray || (langComponentArray.length === 0))
    return LanguageSwitching.currentLanguage_;

  // The language component should have length of either two or three.
  if (langComponentArray[0].length !== 2 && langComponentArray[0].length !== 3)
    return LanguageSwitching.currentLanguage_;

  return nodeLevelLanguage;
};

/**
 * Returns a unicode-aware substring of text from startIndex to endIndex.
 * @param {string} text
 * @param {number} startIndex
 * @param {number} endIndex
 * @return {string}
 */
LanguageSwitching.buildOutputString = function(text, startIndex, endIndex) {
  var result = '';
  var textSymbolArray = [...text];
  for (var i = startIndex; i < endIndex; ++i) {
    result += textSymbolArray[i];
  }
  return result;
};

// TODO(akihiroota): Some languages may have the same language code, but be
// distinctly different. For example, there are some dialects of Chinese that
// are very different from each other. For these cases, comparing just the
// language components is not enough to differentiate the languages.
/**
 * Returns true if newLanguage is different than current language.
 * Only compares the language components of the language code.
 * Note: Language code validation is the responsibility of the caller. This
 * function assumes valid language codes.
 * Ex: 'fr-fr' and 'fr-ca' have the same language component, but different
 * locales. We would return false in the above case. Ex: 'fr-ca' and 'en-ca' are
 * different language components, but same locales. We would return true in the
 * above case.
 * @param {string} newLanguage The language for current output.
 * @return {boolean}
 */
LanguageSwitching.didLanguageSwitch = function(newLanguage) {
  // Compare language components of current and new language codes.
  var newLanguageComponents = newLanguage.split('-');
  var currentLanguageComponents = LanguageSwitching.currentLanguage_.split('-');
  if (newLanguageComponents[0] !== currentLanguageComponents[0])
    return true;
  return false;
};

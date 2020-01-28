// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_WINDOWS_SPELL_CHECKER_H_
#define COMPONENTS_SPELLCHECK_BROWSER_WINDOWS_SPELL_CHECKER_H_

#include <spellcheck.h>
#include <wrl/client.h>

#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace base {
class SingleThreadTaskRunner;
}

// WindowsSpellChecker class is used to store all the COM objects and
// control their lifetime. The class also provides wrappers for
// ISpellCheckerFactory and ISpellChecker APIs. All COM calls are on the
// background thread.
class WindowsSpellChecker {
 public:
  WindowsSpellChecker(
      const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  WindowsSpellChecker(const WindowsSpellChecker&) = delete;

  WindowsSpellChecker& operator=(const WindowsSpellChecker&) = delete;

  ~WindowsSpellChecker();

  void CreateSpellChecker(const std::string& lang_tag,
                          base::OnceCallback<void(bool)> callback);

  void DisableSpellChecker(const std::string& lang_tag);

  void RequestTextCheckForAllLanguages(
      int document_tag,
      const base::string16& text,
      spellcheck_platform::TextCheckCompleteCallback callback);

  void GetPerLanguageSuggestions(
      const base::string16& word,
      spellcheck_platform::GetSuggestionsCallback callback);

  void AddWordForAllLanguages(const base::string16& word);

  void RemoveWordForAllLanguages(const base::string16& word);

  void IgnoreWordForAllLanguages(const base::string16& word);

  void RecordChromeLocalesStats(const std::vector<std::string> chrome_locales,
                                SpellCheckHostMetrics* metrics);

  void RecordSpellcheckLocalesStats(
      const std::vector<std::string> spellcheck_locales,
      SpellCheckHostMetrics* metrics);

  void IsLanguageSupported(const std::string& lang_tag,
                           base::OnceCallback<void(bool)> callback);

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
  // Retrieve language tags for installed Windows language packs that also have
  // spellchecking support.
  void RetrieveSupportedWindowsPreferredLanguages(
      spellcheck_platform::RetrieveSupportedLanguagesCompleteCallback callback);
#endif  // BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)

 private:
  void CreateSpellCheckerFactoryInBackgroundThread();

  // Creates ISpellchecker for given language |lang_tag| and run callback with
  // the creation result. This function must run on the background thread.
  void CreateSpellCheckerWithCallbackInBackgroundThread(
      const std::string& lang_tag,
      base::OnceCallback<void(bool)> callback);

  // Removes the Windows Spellchecker for the given language |lang_tag|. This
  // function must run on the background thread.
  void DisableSpellCheckerInBackgroundThread(const std::string& lang_tag);

  // Request spell checking of string |text| for all available spellchecking
  // languages and run callback with spellchecking results. This function must
  // run on the background thread.
  void RequestTextCheckForAllLanguagesInBackgroundThread(
      int document_tag,
      const base::string16& text,
      spellcheck_platform::TextCheckCompleteCallback callback);

  void GetPerLanguageSuggestionsInBackgroundThread(
      const base::string16& word,
      spellcheck_platform::GetSuggestionsCallback callback);

  // Fills the given vector |optional_suggestions| with a number (up to
  // kMaxSuggestions) of suggestions for the string |wrong_word| of language
  // |lang_tag|. This function must run on the background thread.
  void FillSuggestionListInBackgroundThread(
      const std::string& lang_tag,
      const base::string16& wrong_word,
      std::vector<base::string16>* optional_suggestions);

  void AddWordForAllLanguagesInBackgroundThread(const base::string16& word);

  void RemoveWordForAllLanguagesInBackgroundThread(const base::string16& word);

  void IgnoreWordForAllLanguagesInBackgroundThread(const base::string16& word);

  // Returns true if a spellchecker is available for the given language
  // |lang_tag|. This function must run on the background thread.
  bool IsLanguageSupportedInBackgroundThread(const std::string& lang_tag);

  // Checks if a spellchecker is available for the given language |lang_tag| and
  // posts the result to a callback on the main thread.
  void IsLanguageSupportedWithCallbackInBackgroundThread(
      const std::string& lang_tag,
      base::OnceCallback<void(bool)> callback);

  // Returns true if ISpellCheckerFactory has been initialized.
  bool IsSpellCheckerFactoryInitialized();

  // Returns true if the ISpellChecker has been initialized for given laugnage
  // |lang_tag|.
  bool SpellCheckerReady(const std::string& lang_tag);

  // Returns the ISpellChecker pointer for given language |lang_tag|.
  Microsoft::WRL::ComPtr<ISpellChecker> GetSpellChecker(
      const std::string& lang_tag);

  // Records statistics about spell check support for the user's Chrome locales.
  void RecordChromeLocalesStatsInBackgroundThread(
      const std::vector<std::string> chrome_locales,
      SpellCheckHostMetrics* metrics);

  // Records statistics about which spell checker supports which of the user's
  // enabled spell check locales.
  void RecordSpellcheckLocalesStatsInBackgroundThread(
      const std::vector<std::string> spellcheck_locales,
      SpellCheckHostMetrics* metrics);

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
  // Async retrieval of supported preferred languages.
  void RetrieveSupportedWindowsPreferredLanguagesInBackgroundThread(
      spellcheck_platform::RetrieveSupportedLanguagesCompleteCallback callback);
#endif  // BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)

  // Sorts the specified locales into 4 buckets of spell check support (both,
  // Hunspell only, native only, and none). The results are returned as out
  // variables.
  LocalesSupportInfo DetermineLocalesSupportInBackgroundThread(
      const std::vector<std::string>& locales);

  // Spellchecker objects are owned by WindowsSpellChecker class.
  Microsoft::WRL::ComPtr<ISpellCheckerFactory> spell_checker_factory_;
  std::map<std::string, Microsoft::WRL::ComPtr<ISpellChecker>>
      spell_checker_map_;

  // |main_task_runner_| is running on the main thread, which is used to post
  // task to the main thread from the background thread.
  // |background_task_runner_| is running on the background thread, which is
  // used to post task to the background thread from main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
  base::WeakPtrFactory<WindowsSpellChecker> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_WINDOWS_SPELL_CHECKER_H_

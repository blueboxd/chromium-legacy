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
#include "components/spellcheck/browser/platform_spell_checker.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace base {
class SingleThreadTaskRunner;
}

// Class used to store all the COM objects and control their lifetime. The class
// also provides wrappers for ISpellCheckerFactory and ISpellChecker APIs. All
// COM calls are executed on the background thread.
class WindowsSpellChecker : public PlatformSpellChecker {
 public:
  WindowsSpellChecker(
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  WindowsSpellChecker(const WindowsSpellChecker&) = delete;

  WindowsSpellChecker& operator=(const WindowsSpellChecker&) = delete;

  ~WindowsSpellChecker() override;

  void CreateSpellChecker(const std::string& lang_tag,
                          base::OnceCallback<void(bool)> callback);

  void DisableSpellChecker(const std::string& lang_tag);

  void RequestTextCheck(
      int document_tag,
      const base::string16& text,
      spellcheck_platform::TextCheckCompleteCallback callback) override;

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

 private:
  // Private inner class that handles calls to the native Windows APIs. All
  // invocations of these methods must be posted to the same COM
  // |SingleThreadTaskRunner|. This is enforced by checks that all methods run
  // on the given |SingleThreadTaskRunner|.
  class BackgroundHelper {
   public:
    BackgroundHelper(
        scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);
    ~BackgroundHelper();

    // Creates the native spell check factory, which is the main entry point to
    // the native spell checking APIs.
    void CreateSpellCheckerFactory();

    // Creates a native |ISpellchecker| for the given language |lang_tag| and
    // returns a boolean indicating success.
    bool CreateSpellChecker(const std::string& lang_tag);

    // Removes the native spell checker for the given language |lang_tag| from
    // the map of active spell checkers.
    void DisableSpellChecker(const std::string& lang_tag);

    // Requests spell checking of string |text| for all active spell checkers
    // (all languages) and returns a vector of |SpellCheckResult| containing the
    // results.
    std::vector<SpellCheckResult> RequestTextCheckForAllLanguages(
        int document_tag,
        const base::string16& text);

    // Gets spelling suggestions for |word| from all active spell checkers (all
    // languages), keeping the suggestions separate per language, and returns
    // the results in a vector of vector of strings.
    spellcheck::PerLanguageSuggestions GetPerLanguageSuggestions(
        const base::string16& word);

    // Fills the given vector |optional_suggestions| with a number (up to
    // kMaxSuggestions) of suggestions for the string |wrong_word| using the
    // native spell checker for language |lang_tag|.
    void FillSuggestionList(const std::string& lang_tag,
                            const base::string16& wrong_word,
                            std::vector<base::string16>* optional_suggestions);

    // Adds |word| to the native dictionary of all active spell checkers (all
    // languages).
    void AddWordForAllLanguages(const base::string16& word);

    // Removes |word| from the native dictionary of all active spell checkers
    // (all languages). This requires a newer version of the native spell
    // check APIs, so it may be a no-op on older Windows versions.
    void RemoveWordForAllLanguages(const base::string16& word);

    // Adds |word| to the ignore list of all active spell checkers (all
    // languages).
    void IgnoreWordForAllLanguages(const base::string16& word);

    // Returns |true| if a native spell checker is available for the given
    // language |lang_tag|. This is based on the installed language packs in the
    // OS settings.
    bool IsLanguageSupported(const std::string& lang_tag);

    // Returns |true| if an |ISpellCheckerFactory| has been initialized.
    bool IsSpellCheckerFactoryInitialized();

    // Returns |true| if an |ISpellChecker| has been initialized for the given
    // language |lang_tag|.
    bool SpellCheckerReady(const std::string& lang_tag);

    // Returns the |ISpellChecker| pointer for the given language |lang_tag|.
    Microsoft::WRL::ComPtr<ISpellChecker> GetSpellChecker(
        const std::string& lang_tag);

    // Records metrics about spell check support for the user's Chrome locales.
    void RecordChromeLocalesStats(const std::vector<std::string> chrome_locales,
                                  SpellCheckHostMetrics* metrics);

    // Records metrics about spell check support for the user's enabled spell
    // check locales.
    void RecordSpellcheckLocalesStats(
        const std::vector<std::string> spellcheck_locales,
        SpellCheckHostMetrics* metrics);

    // Sorts the given locales into four buckets based on spell check support
    // (both native and Hunspell, Hunspell only, native only, none).
    LocalesSupportInfo DetermineLocalesSupport(
        const std::vector<std::string>& locales);

   private:
    // The native factory to interact with spell check APIs.
    Microsoft::WRL::ComPtr<ISpellCheckerFactory> spell_checker_factory_;

    // The map of active spell checkers. Each entry maps a language tag to an
    // |ISpellChecker| (there is one |ISpellChecker| per language).
    std::map<std::string, Microsoft::WRL::ComPtr<ISpellChecker>>
        spell_checker_map_;

    // Task runner only used to enforce valid sequencing.
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
  };  // class BackgroundHelper

  // COM-enabled, single-thread task runner used to post invocations of
  // BackgroundHelper methods to interact with spell check native APIs.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;

  // Instance of the background helper to invoke native APIs on the COM-enabled
  // background thread.
  std::unique_ptr<BackgroundHelper> background_helper_;
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_WINDOWS_SPELL_CHECKER_H_

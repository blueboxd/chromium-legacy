// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <string>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/windows_spell_checker.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace spellcheck_platform {

namespace {

// Create WindowsSpellChecker class with static storage duration that is only
// constructed on first access and never invokes the destructor.
std::unique_ptr<WindowsSpellChecker>& GetWindowsSpellChecker() {
  static base::NoDestructor<std::unique_ptr<WindowsSpellChecker>>
      win_spell_checker(std::make_unique<WindowsSpellChecker>(
          base::ThreadTaskRunnerHandle::Get(),
          base::CreateCOMSTATaskRunner(
              {base::ThreadPool(), base::MayBlock()})));
  return *win_spell_checker;
}

}  // anonymous namespace

bool SpellCheckerAvailable() {
  return true;
}

void PlatformSupportsLanguage(const std::string& lang_tag,
                              base::OnceCallback<void(bool)> callback) {
  GetWindowsSpellChecker()->IsLanguageSupported(lang_tag, std::move(callback));
}

void SetLanguage(const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback) {
  GetWindowsSpellChecker()->CreateSpellChecker(lang_to_set,
                                               std::move(callback));
}

void DisableLanguage(const std::string& lang_to_disable) {
  GetWindowsSpellChecker()->DisableSpellChecker(lang_to_disable);
}

bool CheckSpelling(const base::string16& word_to_check, int tag) {
  return true;  // Not used in the Windows native spell checker.
}

void FillSuggestionList(const base::string16& wrong_word,
                        std::vector<base::string16>* optional_suggestions) {
  // Not used in the Windows native spell checker.
}

void RequestTextCheck(int document_tag,
                      const base::string16& text,
                      TextCheckCompleteCallback callback) {
  GetWindowsSpellChecker()->RequestTextCheckForAllLanguages(
      document_tag, text, std::move(callback));
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
void GetPerLanguageSuggestions(const base::string16& word,
                               GetSuggestionsCallback callback) {
  GetWindowsSpellChecker()->GetPerLanguageSuggestions(word,
                                                      std::move(callback));
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

void AddWord(const base::string16& word) {
  GetWindowsSpellChecker()->AddWordForAllLanguages(word);
}

void RemoveWord(const base::string16& word) {
  GetWindowsSpellChecker()->RemoveWordForAllLanguages(word);
}

void IgnoreWord(const base::string16& word) {
  GetWindowsSpellChecker()->IgnoreWordForAllLanguages(word);
}

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
  // Not used in Windows
}

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
void RetrieveSupportedWindowsPreferredLanguages(
    RetrieveSupportedLanguagesCompleteCallback callback) {
  GetWindowsSpellChecker()->RetrieveSupportedWindowsPreferredLanguages(
      std::move(callback));
}
#endif  // BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK

int GetDocumentTag() {
  return 1;  // Not used in Windows
}

void CloseDocumentWithTag(int tag) {
  // Not implemented since Windows spellchecker doesn't have this concept
}

bool SpellCheckerProvidesPanel() {
  return false;  // Windows doesn't have a spelling panel
}

bool SpellingPanelVisible() {
  return false;  // Windows doesn't have a spelling panel
}

void ShowSpellingPanel(bool show) {
  // Not implemented since Windows doesn't have spelling panel like Mac
}

void UpdateSpellingPanelWithMisspelledWord(const base::string16& word) {
  // Not implemented since Windows doesn't have spelling panel like Mac
}

void RecordChromeLocalesStats(const std::vector<std::string> chrome_locales,
                              SpellCheckHostMetrics* metrics) {
  if (spellcheck::WindowsVersionSupportsSpellchecker()) {
    GetWindowsSpellChecker()->RecordChromeLocalesStats(
        std::move(chrome_locales), metrics);
  }
}

void RecordSpellcheckLocalesStats(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  if (spellcheck::WindowsVersionSupportsSpellchecker()) {
    GetWindowsSpellChecker()->RecordSpellcheckLocalesStats(
        std::move(spellcheck_locales), metrics);
  }
}

}  // namespace spellcheck_platform

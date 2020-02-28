// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/windows_spell_checker.h"

#include <objidl.h>
#include <spellcheck.h>
#include <windows.foundation.collections.h>
#include <windows.globalization.h>
#include <windows.system.userprofile.h>
#include <winnls.h>  // ResolveLocaleName
#include <wrl/client.h>

#include <algorithm>
#include <codecvt>
#include <locale>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"

WindowsSpellChecker::BackgroundHelper::BackgroundHelper(
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : background_task_runner_(std::move(background_task_runner)) {}

WindowsSpellChecker::BackgroundHelper::~BackgroundHelper() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
}

void WindowsSpellChecker::BackgroundHelper::CreateSpellCheckerFactory() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

  if (!spellcheck::WindowsVersionSupportsSpellchecker() ||
      FAILED(::CoCreateInstance(__uuidof(::SpellCheckerFactory), nullptr,
                                (CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER),
                                IID_PPV_ARGS(&spell_checker_factory_)))) {
    spell_checker_factory_ = nullptr;
  }
}

bool WindowsSpellChecker::BackgroundHelper::CreateSpellChecker(
    const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized())
    return false;

  if (SpellCheckerReady(lang_tag))
    return true;

  if (!IsLanguageSupported(lang_tag))
    return false;

  Microsoft::WRL::ComPtr<ISpellChecker> spell_checker;
  std::wstring bcp47_language_tag = base::UTF8ToWide(lang_tag);
  HRESULT hr = spell_checker_factory_->CreateSpellChecker(
      bcp47_language_tag.c_str(), &spell_checker);

  if (SUCCEEDED(hr)) {
    spell_checker_map_.insert({lang_tag, spell_checker});
    return true;
  }

  return false;
}

void WindowsSpellChecker::BackgroundHelper::DisableSpellChecker(
    const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized())
    return;

  auto it = spell_checker_map_.find(lang_tag);
  if (it != spell_checker_map_.end()) {
    spell_checker_map_.erase(it);
  }
}

std::vector<SpellCheckResult>
WindowsSpellChecker::BackgroundHelper::RequestTextCheckForAllLanguages(
    int document_tag,
    const base::string16& text) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  // Construct a map to store spellchecking results. The key of the map is a
  // tuple which contains the start index and the word length of the misspelled
  // word. The value of the map is a vector which contains suggestion lists for
  // each available language. This allows to quickly see if all languages agree
  // about a misspelling, and makes it easier to evenly pick suggestions from
  // all the different languages.
  std::map<std::tuple<ULONG, ULONG>, spellcheck::PerLanguageSuggestions>
      result_map;
  std::wstring word_to_check_wide(base::UTF16ToWide(text));

  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    Microsoft::WRL::ComPtr<IEnumSpellingError> spelling_errors;

    HRESULT hr = it->second->ComprehensiveCheck(word_to_check_wide.c_str(),
                                                &spelling_errors);
    if (SUCCEEDED(hr) && spelling_errors) {
      do {
        Microsoft::WRL::ComPtr<ISpellingError> spelling_error;
        ULONG start_index = 0;
        ULONG error_length = 0;
        CORRECTIVE_ACTION action = CORRECTIVE_ACTION_NONE;
        hr = spelling_errors->Next(&spelling_error);
        if (SUCCEEDED(hr) && spelling_error &&
            SUCCEEDED(spelling_error->get_StartIndex(&start_index)) &&
            SUCCEEDED(spelling_error->get_Length(&error_length)) &&
            SUCCEEDED(spelling_error->get_CorrectiveAction(&action)) &&
            (action == CORRECTIVE_ACTION_GET_SUGGESTIONS ||
             action == CORRECTIVE_ACTION_REPLACE)) {
          std::vector<base::string16> suggestions;
          FillSuggestionList(it->first, text.substr(start_index, error_length),
                             &suggestions);

          result_map[std::tuple<ULONG, ULONG>(start_index, error_length)]
              .push_back(suggestions);
        }
      } while (hr == S_OK);
    }
  }

  std::vector<SpellCheckResult> final_results;

  for (auto it = result_map.begin(); it != result_map.end();) {
    if (it->second.size() < spell_checker_map_.size()) {
      // Some languages considered this correctly spelled, so ignore this
      // result.
      it = result_map.erase(it);
    } else {
      std::vector<base::string16> evenly_filled_suggestions;
      spellcheck::FillSuggestions(/*suggestions_list=*/it->second,
                                  &evenly_filled_suggestions);
      final_results.push_back(SpellCheckResult(
          SpellCheckResult::Decoration::SPELLING, std::get<0>(it->first),
          std::get<1>(it->first), evenly_filled_suggestions));
      ++it;
    }
  }

  return final_results;
}

spellcheck::PerLanguageSuggestions
WindowsSpellChecker::BackgroundHelper::GetPerLanguageSuggestions(
    const base::string16& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  spellcheck::PerLanguageSuggestions suggestions;
  std::vector<base::string16> language_suggestions;

  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    language_suggestions.clear();
    FillSuggestionList(it->first, word, &language_suggestions);
    suggestions.push_back(language_suggestions);
  }

  return suggestions;
}

void WindowsSpellChecker::BackgroundHelper::FillSuggestionList(
    const std::string& lang_tag,
    const base::string16& wrong_word,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  std::wstring word_wide(base::UTF16ToWide(wrong_word));

  Microsoft::WRL::ComPtr<IEnumString> suggestions;
  HRESULT hr =
      GetSpellChecker(lang_tag)->Suggest(word_wide.c_str(), &suggestions);

  // Populate the vector of WideStrings.
  while (hr == S_OK) {
    base::win::ScopedCoMem<wchar_t> suggestion;
    hr = suggestions->Next(1, &suggestion, nullptr);
    if (hr == S_OK) {
      base::string16 utf16_suggestion;
      if (base::WideToUTF16(suggestion.get(), wcslen(suggestion),
                            &utf16_suggestion)) {
        optional_suggestions->push_back(utf16_suggestion);
      }
    }
  }
}

void WindowsSpellChecker::BackgroundHelper::AddWordForAllLanguages(
    const base::string16& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_add_wide(base::UTF16ToWide(word));
    it->second->Add(word_to_add_wide.c_str());
  }
}

void WindowsSpellChecker::BackgroundHelper::RemoveWordForAllLanguages(
    const base::string16& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_remove_wide(base::UTF16ToWide(word));
    Microsoft::WRL::ComPtr<ISpellChecker2> spell_checker_2;
    it->second->QueryInterface(IID_PPV_ARGS(&spell_checker_2));
    if (spell_checker_2 != nullptr) {
      spell_checker_2->Remove(word_to_remove_wide.c_str());
    }
  }
}

void WindowsSpellChecker::BackgroundHelper::IgnoreWordForAllLanguages(
    const base::string16& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_ignore_wide(base::UTF16ToWide(word));
    it->second->Ignore(word_to_ignore_wide.c_str());
  }
}

bool WindowsSpellChecker::BackgroundHelper::IsLanguageSupported(
    const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed; no language is supported.
    return false;
  }

  BOOL is_language_supported = (BOOL) false;
  std::wstring bcp47_language_tag = base::UTF8ToWide(lang_tag);

  HRESULT hr = spell_checker_factory_->IsSupported(bcp47_language_tag.c_str(),
                                                   &is_language_supported);
  return SUCCEEDED(hr) && is_language_supported;
}

LocalesSupportInfo
WindowsSpellChecker::BackgroundHelper::DetermineLocalesSupport(
    const std::vector<std::string>& locales) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  size_t locales_supported_by_hunspell_and_native = 0;
  size_t locales_supported_by_hunspell_only = 0;
  size_t locales_supported_by_native_only = 0;
  size_t unsupported_locales = 0;

  for (const auto& lang : locales) {
    bool hunspell_support =
        !spellcheck::GetCorrespondingSpellCheckLanguage(lang).empty();
    bool native_support = this->IsLanguageSupported(lang);

    if (hunspell_support && native_support) {
      locales_supported_by_hunspell_and_native++;
    } else if (hunspell_support && !native_support) {
      locales_supported_by_hunspell_only++;
    } else if (!hunspell_support && native_support) {
      locales_supported_by_native_only++;
    } else {
      unsupported_locales++;
    }
  }

  return LocalesSupportInfo{locales_supported_by_hunspell_and_native,
                            locales_supported_by_hunspell_only,
                            locales_supported_by_native_only,
                            unsupported_locales};
}

bool WindowsSpellChecker::BackgroundHelper::IsSpellCheckerFactoryInitialized() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return spell_checker_factory_ != nullptr;
}

bool WindowsSpellChecker::BackgroundHelper::SpellCheckerReady(
    const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return spell_checker_map_.find(lang_tag) != spell_checker_map_.end();
}

Microsoft::WRL::ComPtr<ISpellChecker>
WindowsSpellChecker::BackgroundHelper::GetSpellChecker(
    const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(SpellCheckerReady(lang_tag));
  return spell_checker_map_.find(lang_tag)->second;
}

void WindowsSpellChecker::BackgroundHelper::RecordChromeLocalesStats(
    const std::vector<std::string> chrome_locales,
    SpellCheckHostMetrics* metrics) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(metrics);

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  const auto& locales_info = DetermineLocalesSupport(chrome_locales);
  metrics->RecordAcceptLanguageStats(locales_info);
}

void WindowsSpellChecker::BackgroundHelper::RecordSpellcheckLocalesStats(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(metrics);

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  const auto& locales_info = DetermineLocalesSupport(spellcheck_locales);
  metrics->RecordSpellcheckLanguageStats(locales_info);
}

WindowsSpellChecker::WindowsSpellChecker(
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : background_task_runner_(background_task_runner) {
  background_helper_ = std::make_unique<WindowsSpellChecker::BackgroundHelper>(
      std::move(background_task_runner));

  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundHelper::CreateSpellCheckerFactory,
                                base::Unretained(background_helper_.get())));
}

WindowsSpellChecker::~WindowsSpellChecker() {
  background_task_runner_->DeleteSoon(FROM_HERE, std::move(background_helper_));
}

void WindowsSpellChecker::CreateSpellChecker(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&BackgroundHelper::CreateSpellChecker,
                     base::Unretained(background_helper_.get()), lang_tag),
      std::move(callback));
}

void WindowsSpellChecker::DisableSpellChecker(const std::string& lang_tag) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundHelper::DisableSpellChecker,
                     base::Unretained(background_helper_.get()), lang_tag));
}

void WindowsSpellChecker::RequestTextCheck(
    int document_tag,
    const base::string16& text,
    spellcheck_platform::TextCheckCompleteCallback callback) {
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&BackgroundHelper::RequestTextCheckForAllLanguages,
                     base::Unretained(background_helper_.get()), document_tag,
                     text),
      std::move(callback));
}

void WindowsSpellChecker::GetPerLanguageSuggestions(
    const base::string16& word,
    spellcheck_platform::GetSuggestionsCallback callback) {
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&BackgroundHelper::GetPerLanguageSuggestions,
                     base::Unretained(background_helper_.get()), word),
      std::move(callback));
}

void WindowsSpellChecker::AddWordForAllLanguages(const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundHelper::AddWordForAllLanguages,
                     base::Unretained(background_helper_.get()), word));
}

void WindowsSpellChecker::RemoveWordForAllLanguages(
    const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundHelper::RemoveWordForAllLanguages,
                     base::Unretained(background_helper_.get()), word));
}

void WindowsSpellChecker::IgnoreWordForAllLanguages(
    const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundHelper::IgnoreWordForAllLanguages,
                     base::Unretained(background_helper_.get()), word));
}

void WindowsSpellChecker::IsLanguageSupported(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&BackgroundHelper::IsLanguageSupported,
                     base::Unretained(background_helper_.get()), lang_tag),
      std::move(callback));
}

void WindowsSpellChecker::RecordChromeLocalesStats(
    const std::vector<std::string> chrome_locales,
    SpellCheckHostMetrics* metrics) {
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundHelper::RecordChromeLocalesStats,
                                base::Unretained(background_helper_.get()),
                                std::move(chrome_locales), metrics));
}

void WindowsSpellChecker::RecordSpellcheckLocalesStats(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundHelper::RecordSpellcheckLocalesStats,
                                base::Unretained(background_helper_.get()),
                                std::move(spellcheck_locales), metrics));
}

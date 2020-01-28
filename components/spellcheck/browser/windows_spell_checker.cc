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

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
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

WindowsSpellChecker::WindowsSpellChecker(
    const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : main_task_runner_(main_task_runner),
      background_task_runner_(background_task_runner) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::CreateSpellCheckerFactoryInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr()));
}

WindowsSpellChecker::~WindowsSpellChecker() = default;

void WindowsSpellChecker::CreateSpellChecker(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         CreateSpellCheckerWithCallbackInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), lang_tag,
                     std::move(callback)));
}

void WindowsSpellChecker::DisableSpellChecker(const std::string& lang_tag) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::DisableSpellCheckerInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), lang_tag));
}

void WindowsSpellChecker::RequestTextCheckForAllLanguages(
    int document_tag,
    const base::string16& text,
    spellcheck_platform::TextCheckCompleteCallback callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         RequestTextCheckForAllLanguagesInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), document_tag, text,
                     std::move(callback)));
}

void WindowsSpellChecker::GetPerLanguageSuggestions(
    const base::string16& word,
    spellcheck_platform::GetSuggestionsCallback callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::GetPerLanguageSuggestionsInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), word, std::move(callback)));
}

void WindowsSpellChecker::AddWordForAllLanguages(const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::AddWordForAllLanguagesInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), word));
}

void WindowsSpellChecker::RemoveWordForAllLanguages(
    const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::RemoveWordForAllLanguagesInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), word));
}

void WindowsSpellChecker::IgnoreWordForAllLanguages(
    const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::IgnoreWordForAllLanguagesInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), word));
}

void WindowsSpellChecker::IsLanguageSupported(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         IsLanguageSupportedWithCallbackInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), lang_tag,
                     std::move(callback)));
}

void WindowsSpellChecker::RecordChromeLocalesStats(
    const std::vector<std::string> chrome_locales,
    SpellCheckHostMetrics* metrics) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::RecordChromeLocalesStatsInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), std::move(chrome_locales), metrics));
}

void WindowsSpellChecker::RecordSpellcheckLocalesStats(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::RecordSpellcheckLocalesStatsInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), std::move(spellcheck_locales),
          metrics));
}

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
void WindowsSpellChecker::RetrieveSupportedWindowsPreferredLanguages(
    spellcheck_platform::RetrieveSupportedLanguagesCompleteCallback callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::
              RetrieveSupportedWindowsPreferredLanguagesInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
#endif  // BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK

void WindowsSpellChecker::CreateSpellCheckerFactoryInBackgroundThread() {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

  if (!spellcheck::WindowsVersionSupportsSpellchecker() ||
      FAILED(::CoCreateInstance(__uuidof(::SpellCheckerFactory), nullptr,
                                (CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER),
                                IID_PPV_ARGS(&spell_checker_factory_)))) {
    spell_checker_factory_ = nullptr;
  }
}

void WindowsSpellChecker::CreateSpellCheckerWithCallbackInBackgroundThread(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  bool result = false;
  if (IsSpellCheckerFactoryInitialized()) {
    if (SpellCheckerReady(lang_tag)) {
      result = true;
    } else if (IsLanguageSupportedInBackgroundThread(lang_tag)) {
      Microsoft::WRL::ComPtr<ISpellChecker> spell_checker;
      std::wstring bcp47_language_tag = base::UTF8ToWide(lang_tag);
      HRESULT hr = spell_checker_factory_->CreateSpellChecker(
          bcp47_language_tag.c_str(), &spell_checker);
      if (SUCCEEDED(hr)) {
        spell_checker_map_.insert({lang_tag, spell_checker});
        result = true;
      }
    }
  }

  // Run the callback with result on the main thread.
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
}

void WindowsSpellChecker::DisableSpellCheckerInBackgroundThread(
    const std::string& lang_tag) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  if (!IsSpellCheckerFactoryInitialized())
    return;

  auto it = spell_checker_map_.find(lang_tag);
  if (it != spell_checker_map_.end()) {
    spell_checker_map_.erase(it);
  }
}

void WindowsSpellChecker::RequestTextCheckForAllLanguagesInBackgroundThread(
    int document_tag,
    const base::string16& text,
    spellcheck_platform::TextCheckCompleteCallback callback) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

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
          FillSuggestionListInBackgroundThread(
              it->first, text.substr(start_index, error_length), &suggestions);

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

  // Runs the callback on the main thread after spellcheck completed.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), final_results));
}

void WindowsSpellChecker::GetPerLanguageSuggestionsInBackgroundThread(
    const base::string16& word,
    spellcheck_platform::GetSuggestionsCallback callback) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  spellcheck::PerLanguageSuggestions suggestions;
  std::vector<base::string16> language_suggestions;

  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    language_suggestions.clear();
    FillSuggestionListInBackgroundThread(it->first, word,
                                         &language_suggestions);
    suggestions.push_back(language_suggestions);
  }

  // Runs the callback on the main thread after spellcheck completed.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(suggestions)));
}

void WindowsSpellChecker::FillSuggestionListInBackgroundThread(
    const std::string& lang_tag,
    const base::string16& wrong_word,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

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

void WindowsSpellChecker::AddWordForAllLanguagesInBackgroundThread(
    const base::string16& word) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_add_wide(base::UTF16ToWide(word));
    it->second->Add(word_to_add_wide.c_str());
  }
}

void WindowsSpellChecker::RemoveWordForAllLanguagesInBackgroundThread(
    const base::string16& word) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
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

void WindowsSpellChecker::IgnoreWordForAllLanguagesInBackgroundThread(
    const base::string16& word) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_ignore_wide(base::UTF16ToWide(word));
    it->second->Ignore(word_to_ignore_wide.c_str());
  }
}

bool WindowsSpellChecker::IsLanguageSupportedInBackgroundThread(
    const std::string& lang_tag) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

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

void WindowsSpellChecker::IsLanguageSupportedWithCallbackInBackgroundThread(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  bool result = IsLanguageSupportedInBackgroundThread(lang_tag);

  // Run the callback with result on the main thread.
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
}

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
void WindowsSpellChecker::
    RetrieveSupportedWindowsPreferredLanguagesInBackgroundThread(
        spellcheck_platform::RetrieveSupportedLanguagesCompleteCallback
            callback) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  std::vector<std::string> supported_languages;

  if (IsSpellCheckerFactoryInitialized() &&
      // IGlobalizationPreferencesStatics is only available on Win8 and above.
      spellcheck::WindowsVersionSupportsSpellchecker() &&
      // Using WinRT and HSTRING.
      base::win::ResolveCoreWinRTDelayload() &&
      base::win::ScopedHString::ResolveCoreWinRTStringDelayload()) {
    Microsoft::WRL::ComPtr<
        ABI::Windows::System::UserProfile::IGlobalizationPreferencesStatics>
        globalization_preferences;

    HRESULT hr = base::win::GetActivationFactory<
        ABI::Windows::System::UserProfile::IGlobalizationPreferencesStatics,
        RuntimeClass_Windows_System_UserProfile_GlobalizationPreferences>(
        &globalization_preferences);
    // Should always succeed under same conditions for which
    // WindowsVersionSupportsSpellchecker returns true.
    DCHECK(SUCCEEDED(hr));
    // Retrieve a vector of Windows preferred languages (that is, installed
    // language packs listed under system Language Settings).
    Microsoft::WRL::ComPtr<
        ABI::Windows::Foundation::Collections::IVectorView<HSTRING>>
        preferred_languages;
    hr = globalization_preferences->get_Languages(&preferred_languages);
    DCHECK(SUCCEEDED(hr));
    uint32_t count = 0;
    hr = preferred_languages->get_Size(&count);
    DCHECK(SUCCEEDED(hr));
    // Expect at least one language pack to be installed by default.
    DCHECK_GE(count, 0u);
    for (uint32_t i = 0; i < count; ++i) {
      HSTRING language;
      hr = preferred_languages->GetAt(i, &language);
      DCHECK(SUCCEEDED(hr));
      base::win::ScopedHString language_scoped(language);
      // Language tags obtained using Windows.Globalization API
      // (zh-Hans-CN e.g.) need to be converted to locale names via
      // ResolveLocaleName before being passed to spell checker API.
      wchar_t locale_name[LOCALE_NAME_MAX_LENGTH];
      const wchar_t* preferred_language =
          base::as_wcstr(base::AsStringPiece16(language_scoped.Get()));
      // ResolveLocaleName should only fail if buffer size insufficient, but
      // it can succeed yet return an empty string for certain language tags
      // such as ht.
      if (!::ResolveLocaleName(preferred_language, locale_name,
                               LOCALE_NAME_MAX_LENGTH) ||
          !*locale_name) {
        DVLOG(1) << "ResolveLocaleName failed or returned empty string for "
                    "preferred language "
                 << preferred_language
                 << ", will try unresolved language name.";
        base::wcslcpy(locale_name, preferred_language, LOCALE_NAME_MAX_LENGTH);
      }
      // See if the language has a dictionary available. Some preferred
      // languages have no spellchecking support (zh-CN e.g.).
      BOOL is_language_supported = FALSE;
      hr = spell_checker_factory_->IsSupported(locale_name,
                                               &is_language_supported);
      DCHECK(SUCCEEDED(hr));
      if (is_language_supported) {
        supported_languages.push_back(base::WideToUTF8(locale_name));
      } else {
        DVLOG(2) << "No platform spellchecking support for locale name "
                 << locale_name;
      }
    }
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), supported_languages));
}
#endif  // #if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)

LocalesSupportInfo
WindowsSpellChecker::DetermineLocalesSupportInBackgroundThread(
    const std::vector<std::string>& locales) {
  size_t locales_supported_by_hunspell_and_native = 0;
  size_t locales_supported_by_hunspell_only = 0;
  size_t locales_supported_by_native_only = 0;
  size_t unsupported_locales = 0;

  for (const auto& lang : locales) {
    bool hunspell_support =
        !spellcheck::GetCorrespondingSpellCheckLanguage(lang).empty();
    bool native_support = this->IsLanguageSupportedInBackgroundThread(lang);

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

bool WindowsSpellChecker::IsSpellCheckerFactoryInitialized() {
  return spell_checker_factory_ != nullptr;
}

bool WindowsSpellChecker::SpellCheckerReady(const std::string& lang_tag) {
  return spell_checker_map_.find(lang_tag) != spell_checker_map_.end();
}

Microsoft::WRL::ComPtr<ISpellChecker> WindowsSpellChecker::GetSpellChecker(
    const std::string& lang_tag) {
  DCHECK(SpellCheckerReady(lang_tag));
  return spell_checker_map_.find(lang_tag)->second;
}

void WindowsSpellChecker::RecordChromeLocalesStatsInBackgroundThread(
    const std::vector<std::string> chrome_locales,
    SpellCheckHostMetrics* metrics) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  DCHECK(metrics);

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  const auto& locales_info =
      DetermineLocalesSupportInBackgroundThread(chrome_locales);
  metrics->RecordAcceptLanguageStats(locales_info);
}

void WindowsSpellChecker::RecordSpellcheckLocalesStatsInBackgroundThread(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  DCHECK(metrics);

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  const auto& locales_info =
      DetermineLocalesSupportInBackgroundThread(spellcheck_locales);
  metrics->RecordSpellcheckLanguageStats(locales_info);
}

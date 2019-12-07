// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

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
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_types.h"
#include "base/win/windows_version.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"

namespace spellcheck_platform {

typedef base::OnceCallback<void(const spellcheck::PerLanguageResult&)>
    PlatformTextCheckCompleteCallback;

namespace {
// WindowsSpellChecker class is used to store all the COM objects and
// control their lifetime. The class also provides wrappers for
// ISpellCheckerFactory and ISpellChecker APIs. All COM calls are on the
// background thread.
class WindowsSpellChecker {
 public:
  WindowsSpellChecker(
      const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  void CreateSpellChecker(const std::string& lang_tag,
                          base::OnceCallback<void(bool)> callback);

  void DisableSpellChecker(const std::string& lang_tag);

  void RequestTextCheckForAllLanguages(
      int document_tag,
      const base::string16& text,
      bool fill_suggestions,
      PlatformTextCheckCompleteCallback callback);

  void GetPerLanguageSuggestions(const base::string16& word,
                                 GetSuggestionsCallback callback);

  void AddWordForAllLanguages(const base::string16& word);

  void RemoveWordForAllLanguages(const base::string16& word);

  void IgnoreWordForAllLanguages(const base::string16& word);

  void RecordMissingLanguagePacksCount(
      const std::vector<std::string> spellcheck_locales,
      SpellCheckHostMetrics* metrics);

  void IsLanguageSupported(const std::string& lang_tag,
                           base::OnceCallback<void(bool)> callback);

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
  // Retrieve language tags for installed Windows language packs that also have
  // spellchecking support.
  void RetrieveSupportedWindowsPreferredLanguages(
      RetrieveSupportedLanguagesCompleteCallback callback);
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
      bool fill_suggestions,
      PlatformTextCheckCompleteCallback callback);

  void GetPerLanguageSuggestionsInBackgroundThread(
      const base::string16& word,
      GetSuggestionsCallback callback);

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

  // Records how many user spellcheck languages are currently not supported by
  // the Windows OS spellchecker due to missing language packs. Must run on the
  // background thread.
  void RecordMissingLanguagePacksCountInBackgroundThread(
      const std::vector<std::string> spellcheck_locales,
      SpellCheckHostMetrics* metrics);

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
  // Async retrieval of supported preferred languages.
  void RetrieveSupportedWindowsPreferredLanguagesInBackgroundThread(
      RetrieveSupportedLanguagesCompleteCallback callback);
#endif  // BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)

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

  DISALLOW_COPY_AND_ASSIGN(WindowsSpellChecker);
};

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
    bool fill_suggestions,
    PlatformTextCheckCompleteCallback callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         RequestTextCheckForAllLanguagesInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), document_tag, text,
                     fill_suggestions, std::move(callback)));
}

void WindowsSpellChecker::GetPerLanguageSuggestions(
    const base::string16& word,
    GetSuggestionsCallback callback) {
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

void WindowsSpellChecker::RecordMissingLanguagePacksCount(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         RecordMissingLanguagePacksCountInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(spellcheck_locales), metrics));
}

#if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)
void WindowsSpellChecker::RetrieveSupportedWindowsPreferredLanguages(
    RetrieveSupportedLanguagesCompleteCallback callback) {
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
    bool fill_suggestions,
    PlatformTextCheckCompleteCallback callback) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  spellcheck::PerLanguageResult results;
  std::wstring word_to_check_wide(base::UTF16ToWide(text));

  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    Microsoft::WRL::ComPtr<IEnumSpellingError> spelling_errors;

    HRESULT hr = it->second->ComprehensiveCheck(word_to_check_wide.c_str(),
                                                &spelling_errors);
    if (SUCCEEDED(hr) && spelling_errors) {
      std::vector<SpellCheckResult> language_results;
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

          if (fill_suggestions) {
            FillSuggestionListInBackgroundThread(
                it->first, text.substr(start_index, error_length),
                &suggestions);
          }

          language_results.push_back(
              SpellCheckResult(SpellCheckResult::Decoration::SPELLING,
                               start_index, error_length, suggestions));
        }
      } while (hr == S_OK);

      results.push_back(language_results);
    }
  }

  // Runs the callback on the main thread after spellcheck completed.
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), results));
}

void WindowsSpellChecker::GetPerLanguageSuggestionsInBackgroundThread(
    const base::string16& word,
    GetSuggestionsCallback callback) {
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
        RetrieveSupportedLanguagesCompleteCallback callback) {
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
      // ResolveLocaleName can only fail if buffer size insufficient.
      ::ResolveLocaleName(
          base::as_wcstr(base::AsStringPiece16(language_scoped.Get())),
          locale_name, LOCALE_NAME_MAX_LENGTH);
      // See if the language has a dictionary available. Some preferred
      // languages have no spellchecking support (zh-CN e.g.).
      BOOL is_language_supported = FALSE;
      hr = spell_checker_factory_->IsSupported(locale_name,
                                               &is_language_supported);
      DCHECK(SUCCEEDED(hr));
      if (is_language_supported)
        supported_languages.push_back(base::WideToUTF8(locale_name));
    }
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), supported_languages));
}
#endif  // #if BUILDFLAG(USE_WINDOWS_PREFERRED_LANGUAGES_FOR_SPELLCHECK)

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

void WindowsSpellChecker::RecordMissingLanguagePacksCountInBackgroundThread(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  DCHECK(metrics);

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  metrics->RecordMissingLanguagePacksCount(
      std::count_if(spellcheck_locales.begin(), spellcheck_locales.end(),
                    [this](const std::string& s) {
                      return !this->IsLanguageSupportedInBackgroundThread(s);
                    }));
}

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

void PlatformCheckComplete(
    std::vector<SpellCheckResult> renderer_results,
    bool fill_suggestions,
    TextCheckCompleteCallback callback,
    const spellcheck::PerLanguageResult& platform_results) {
  // Merge renderer results and platform_results into a single map. The key of
  // the map is a tuple which contains the start index and the word length of
  // the misspelled word. The value of the map is a vector which contains
  // suggestion lists for each available language. This allows to quickly see if
  // all languages agree about a misspelling, and makes it easier to evenly pick
  // suggestions from all the different languages.
  std::vector<SpellCheckResult> final_results;
  std::map<std::tuple<UINT, UINT>, std::vector<std::vector<base::string16>>>
      result_map;
  size_t language_count =
      std::min(renderer_results.size(), size_t(1)) + platform_results.size();

  for (const auto& language_results : platform_results) {
    for (const auto& result : language_results) {
      result_map[std::tuple<UINT, UINT>(result.location, result.length)]
          .push_back(result.replacements);
    }
  }

  for (const auto& result : renderer_results) {
    result_map[std::tuple<UINT, UINT>(result.location, result.length)]
        .push_back(result.replacements);
  }

  for (auto it = result_map.begin(); it != result_map.end();) {
    if (it->second.size() < language_count) {
      // Some languages considered this correctly spelled, so ignore this
      // result.
      it = result_map.erase(it);
    } else {
      if (fill_suggestions) {
        std::vector<std::vector<base::string16>> per_language_suggestions;
        for (auto& suggestions_list : it->second) {
          per_language_suggestions.push_back(suggestions_list);
        }

        std::vector<base::string16> evenly_filled_suggestions;
        spellcheck::FillSuggestions(per_language_suggestions,
                                    &evenly_filled_suggestions);
        final_results.push_back(SpellCheckResult(
            SpellCheckResult::Decoration::SPELLING, std::get<0>(it->first),
            std::get<1>(it->first), evenly_filled_suggestions));
      } else {
        final_results.push_back(
            SpellCheckResult(SpellCheckResult::Decoration::SPELLING,
                             std::get<0>(it->first), std::get<1>(it->first)));
      }
      ++it;
    }
  }

  std::move(callback).Run(final_results);
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
  return true;  // Not used in Windows
}

void FillSuggestionList(const base::string16& wrong_word,
                        std::vector<base::string16>* optional_suggestions) {
  // Not used in Windows.
}

void RequestTextCheck(int document_tag,
                      const base::string16& text,
                      TextCheckCompleteCallback callback) {
  GetWindowsSpellChecker()->RequestTextCheckForAllLanguages(
      document_tag, text, true,
      base::BindOnce(&PlatformCheckComplete, std::vector<SpellCheckResult>(),
                     true, std::move(callback)));
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
void RequestTextCheck(int document_tag,
                      const base::string16& text,
                      const std::vector<SpellCheckResult>& renderer_results,
                      bool fill_suggestions,
                      TextCheckCompleteCallback callback) {
  GetWindowsSpellChecker()->RequestTextCheckForAllLanguages(
      document_tag, text, fill_suggestions,
      base::BindOnce(&PlatformCheckComplete, std::move(renderer_results),
                     fill_suggestions, std::move(callback)));
}

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

void RecordMissingLanguagePacksCount(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  if (spellcheck::WindowsVersionSupportsSpellchecker()) {
    GetWindowsSpellChecker()->RecordMissingLanguagePacksCount(
        std::move(spellcheck_locales), metrics);
  }
}
}  // namespace spellcheck_platform

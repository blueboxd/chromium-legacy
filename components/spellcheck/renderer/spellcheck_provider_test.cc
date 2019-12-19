// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include <memory>

#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/spellcheck_buildflags.h"

FakeTextCheckingCompletion::FakeTextCheckingCompletion(
    FakeTextCheckingResult* result)
    : result_(result) {}

FakeTextCheckingCompletion::~FakeTextCheckingCompletion() {}

void FakeTextCheckingCompletion::DidFinishCheckingText(
    const blink::WebVector<blink::WebTextCheckingResult>& results) {
  ++result_->completion_count_;
}

void FakeTextCheckingCompletion::DidCancelCheckingText() {
  ++result_->completion_count_;
  ++result_->cancellation_count_;
}

FakeSpellCheck::FakeSpellCheck(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheck(embedder_provider) {}

void FakeSpellCheck::SetFakeLanguageCounts(size_t language_count,
                                           size_t enabled_count) {
  use_fake_counts_ = true;
  language_count_ = language_count;
  enabled_language_count_ = enabled_count;
}

size_t FakeSpellCheck::LanguageCount() {
  return use_fake_counts_ ? language_count_ : SpellCheck::LanguageCount();
}

size_t FakeSpellCheck::EnabledLanguageCount() {
  return use_fake_counts_ ? enabled_language_count_
                          : SpellCheck::EnabledLanguageCount();
}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr,
                         new FakeSpellCheck(embedder_provider),
                         embedder_provider) {}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    SpellCheck* spellcheck,
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr, spellcheck, embedder_provider) {}

TestingSpellCheckProvider::~TestingSpellCheckProvider() {
  receiver_.reset();
  // dictionary_update_observer_ must be released before deleting spellcheck_.
  ResetDictionaryUpdateObserverForTesting();
  delete spellcheck_;
}

void TestingSpellCheckProvider::RequestTextChecking(
    const base::string16& text,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion) {
  if (!receiver_.is_bound())
    SetSpellCheckHostForTesting(receiver_.BindNewPipeAndPassRemote());
  SpellCheckProvider::RequestTextChecking(text, std::move(completion));
  base::RunLoop().RunUntilIdle();
}

void TestingSpellCheckProvider::RequestDictionary() {}

void TestingSpellCheckProvider::NotifyChecked(const base::string16& word,
                                              bool misspelled) {}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void TestingSpellCheckProvider::CallSpellingService(
    const base::string16& text,
    CallSpellingServiceCallback callback) {
  OnCallSpellingService(text);
  std::move(callback).Run(true, std::vector<SpellCheckResult>());
}

void TestingSpellCheckProvider::OnCallSpellingService(
    const base::string16& text) {
  ++spelling_service_call_count_;
  if (!text_check_completions_.Lookup(last_identifier_)) {
    ResetResult();
    return;
  }
  text_.assign(text);
  std::unique_ptr<blink::WebTextCheckingCompletion> completion(
      text_check_completions_.Replace(last_identifier_, nullptr));
  text_check_completions_.Remove(last_identifier_);
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 0, 5,
                                   std::vector<blink::WebString>({"hello"})));
  completion->DidFinishCheckingText(results);
  last_request_ = text;
  last_results_ = results;
}

void TestingSpellCheckProvider::ResetResult() {
  text_.clear();
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void TestingSpellCheckProvider::RequestTextCheck(
    const base::string16& text,
    int,
    RequestTextCheckCallback callback) {
  text_check_requests_.push_back(std::make_pair(text, std::move(callback)));
}

void TestingSpellCheckProvider::CheckSpelling(const base::string16&,
                                              int,
                                              CheckSpellingCallback) {
  NOTREACHED();
}

void TestingSpellCheckProvider::FillSuggestionList(const base::string16&,
                                                   FillSuggestionListCallback) {
  NOTREACHED();
}
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
void TestingSpellCheckProvider::GetPerLanguageSuggestions(
    const base::string16& word,
    GetPerLanguageSuggestionsCallback callback) {
  NOTREACHED();
}

void TestingSpellCheckProvider::RequestPartialTextCheck(
    const base::string16& text,
    int route_id,
    const std::vector<SpellCheckResult>& partial_results,
    bool fill_suggestions,
    RequestPartialTextCheckCallback callback) {
  partial_text_check_requests_.push_back(
      std::make_pair(text, std::move(callback)));
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

#if defined(OS_ANDROID)
void TestingSpellCheckProvider::DisconnectSessionBridge() {
  NOTREACHED();
}
#endif

void TestingSpellCheckProvider::SetLastResults(
    const base::string16 last_request,
    blink::WebVector<blink::WebTextCheckingResult>& last_results) {
  last_request_ = last_request;
  last_results_ = last_results;
}

bool TestingSpellCheckProvider::SatisfyRequestFromCache(
    const base::string16& text,
    blink::WebTextCheckingCompletion* completion) {
  return SpellCheckProvider::SatisfyRequestFromCache(text, completion);
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
int TestingSpellCheckProvider::AddCompletionForTest(
    std::unique_ptr<FakeTextCheckingCompletion> completion) {
  return SpellCheckProvider::text_check_completions_.Add(std::move(completion));
}

void TestingSpellCheckProvider::HybridSpellCheckParagraphComplete(
    const base::string16& text,
    int request_id,
    std::vector<SpellCheckResult> renderer_results) {
  if (!receiver_.is_bound())
    SetSpellCheckHostForTesting(receiver_.BindNewPipeAndPassRemote());
  SpellCheckProvider::HybridSpellCheckParagraphComplete(
      text, request_id, std::move(renderer_results));
  base::RunLoop().RunUntilIdle();
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

SpellCheckProviderTest::SpellCheckProviderTest()
    : provider_(&embedder_provider_) {}
SpellCheckProviderTest::~SpellCheckProviderTest() {}

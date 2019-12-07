// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/public/web/web_text_decoration_type.h"

using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebString;
using blink::WebTextCheckingCompletion;
using blink::WebTextCheckingResult;
using blink::WebTextDecorationType;
using blink::WebVector;

static_assert(int(blink::kWebTextDecorationTypeSpelling) ==
                  int(SpellCheckResult::SPELLING),
              "mismatching enums");
static_assert(int(blink::kWebTextDecorationTypeGrammar) ==
                  int(SpellCheckResult::GRAMMAR),
              "mismatching enums");

class SpellCheckProvider::DictionaryUpdateObserverImpl
    : public DictionaryUpdateObserver {
 public:
  explicit DictionaryUpdateObserverImpl(SpellCheckProvider* owner);
  ~DictionaryUpdateObserverImpl() override;

  // DictionaryUpdateObserver:
  void OnDictionaryUpdated(const WebVector<WebString>& words_added) override;

 private:
  SpellCheckProvider* owner_;
};

SpellCheckProvider::DictionaryUpdateObserverImpl::DictionaryUpdateObserverImpl(
    SpellCheckProvider* owner)
    : owner_(owner) {
  owner_->spellcheck_->AddDictionaryUpdateObserver(this);
}

SpellCheckProvider::DictionaryUpdateObserverImpl::
    ~DictionaryUpdateObserverImpl() {
  owner_->spellcheck_->RemoveDictionaryUpdateObserver(this);
}

void SpellCheckProvider::DictionaryUpdateObserverImpl::OnDictionaryUpdated(
    const WebVector<WebString>& words_added) {
  // Clear only cache. Current pending requests should continue as they are.
  owner_->last_request_.clear();
  owner_->last_results_.Assign(
      blink::WebVector<blink::WebTextCheckingResult>());

  // owner_->render_frame() is nullptr in unit tests.
  if (auto* render_frame = owner_->render_frame()) {
    DCHECK(render_frame->GetWebFrame());
    render_frame->GetWebFrame()->RemoveSpellingMarkersUnderWords(words_added);
  }
}

SpellCheckProvider::SpellCheckProvider(
    content::RenderFrame* render_frame,
    SpellCheck* spellcheck,
    service_manager::LocalInterfaceProvider* embedder_provider)
    : content::RenderFrameObserver(render_frame),
      spellcheck_(spellcheck),
      embedder_provider_(embedder_provider) {
  DCHECK(spellcheck_);
  DCHECK(embedder_provider);
  if (render_frame)  // NULL in unit tests.
    render_frame->GetWebFrame()->SetTextCheckClient(this);

  dictionary_update_observer_ =
      std::make_unique<DictionaryUpdateObserverImpl>(this);
}

SpellCheckProvider::~SpellCheckProvider() {
}

void SpellCheckProvider::ResetDictionaryUpdateObserverForTesting() {
  dictionary_update_observer_.reset();
}

spellcheck::mojom::SpellCheckHost& SpellCheckProvider::GetSpellCheckHost() {
  if (spell_check_host_)
    return *spell_check_host_;

  embedder_provider_->GetInterface(
      spell_check_host_.BindNewPipeAndPassReceiver());
  return *spell_check_host_;
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
void SpellCheckProvider::HybridSpellCheckParagraphComplete(
    const base::string16& text,
    const int request_id,
    std::vector<SpellCheckResult> renderer_results) {
  size_t renderer_languages = spellcheck_->EnabledLanguageCount();

  // If there are no renderer results, either 1) there were no languages to
  // check on the renderer side because all languages are supported by the
  // platform; or 2) each word was marked correct by at least 1 language. In
  // case of 2), we can skip the platform check, because we know all words are
  // correctly spelled in at least 1 language.
  // Additionally, if the renderer checked all languages, we can completely skip
  // the browser side.
  if ((renderer_results.empty() && renderer_languages > 0) ||
      renderer_languages == spellcheck_->LanguageCount()) {
    OnRespondTextCheck(request_id, text, renderer_results);
  } else {
    // We need to do a platform check. We must decide whether to find the
    // replacement suggestions now, or wait until the user requests them. If all
    // languages are checked by the platform, find the suggestions now, because
    // the platform spellchecker is fast. If we had to check some languages via
    // Hunspell, then wait until the user requests the replacements, because
    //  Hunspell is extremely slow at finding them and can hang the page for a
    // few seconds if the text is not small.
    GetSpellCheckHost().RequestPartialTextCheck(
        text, routing_id(), std::move(renderer_results),
        renderer_languages == 0,
        base::BindOnce(&SpellCheckProvider::OnRespondTextCheck,
                       weak_factory_.GetWeakPtr(), request_id, text));
  }
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

void SpellCheckProvider::RequestTextChecking(
    const base::string16& text,
    std::unique_ptr<WebTextCheckingCompletion> completion) {
  // Ignore invalid requests.
  if (text.empty() || !HasWordCharacters(text, 0)) {
    completion->DidCancelCheckingText();
    return;
  }

  // Try to satisfy check from cache.
  if (SatisfyRequestFromCache(text, completion.get()))
    return;

  // Send this text to a browser. A browser checks the user profile and send
  // this text to the Spelling service only if a user enables this feature.
  last_request_.clear();
  last_results_.Assign(blink::WebVector<blink::WebTextCheckingResult>());
  last_identifier_ = text_check_completions_.Add(std::move(completion));

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
  if (spellcheck::UseWinHybridSpellChecker()) {
    // Do a first spellcheck pass with Hunspell, then check the rest of the
    // locales with the native spellchecker.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SpellCheck::HybridSpellCheckParagraph, spellcheck_->AsWeakPtr(),
            text,
            base::BindOnce(
                &SpellCheckProvider::HybridSpellCheckParagraphComplete,
                weak_factory_.GetWeakPtr(), text, last_identifier_)));
    return;
  }
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker()) {
    // Text check (unified request for grammar and spell check) is only
    // available for browser process, so we ask the system spellchecker
    // over mojo or return an empty result if the checker is not available.
    GetSpellCheckHost().RequestTextCheck(
        text, routing_id(),
        base::BindOnce(&SpellCheckProvider::OnRespondTextCheck,
                       weak_factory_.GetWeakPtr(), last_identifier_, text));
  }
#endif

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  if (!spellcheck::UseBrowserSpellChecker()) {
    GetSpellCheckHost().CallSpellingService(
        text,
        base::BindOnce(&SpellCheckProvider::OnRespondSpellingService,
                       weak_factory_.GetWeakPtr(), last_identifier_, text));
  }
#endif
}

void SpellCheckProvider::FocusedElementChanged(
    const blink::WebElement& unused) {
#if defined(OS_ANDROID)
  if (!spell_check_host_.is_bound())
    return;

  WebLocalFrame* frame = render_frame()->GetWebFrame();
  WebElement element = frame->GetDocument().IsNull()
                           ? WebElement()
                           : frame->GetDocument().FocusedElement();
  bool enabled = !element.IsNull() && element.IsEditable();
  if (!enabled)
    GetSpellCheckHost().DisconnectSessionBridge();
#endif  // defined(OS_ANDROID)
}

bool SpellCheckProvider::IsSpellCheckingEnabled() const {
  return spellcheck_->IsSpellcheckEnabled();
}

void SpellCheckProvider::CheckSpelling(
    const WebString& text,
    size_t& offset,
    size_t& length,
    WebVector<WebString>* optional_suggestions) {
  base::string16 word = text.Utf16();
  const int kWordStart = 0;

  if (optional_suggestions) {
    spellcheck::PerLanguageSuggestions per_language_suggestions;
    spellcheck_->SpellCheckWord(word.c_str(), kWordStart, word.size(),
                                routing_id(), &offset, &length,
                                &per_language_suggestions);

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
    if (spellcheck::UseWinHybridSpellChecker()) {
      // Also fetch suggestions from the browser process (native spellchecker).
      // This is a synchronous Mojo call, because this method must return
      // synchronously.
      spellcheck::PerLanguageSuggestions browser_suggestions;
      GetSpellCheckHost().GetPerLanguageSuggestions(word, &browser_suggestions);

      per_language_suggestions.reserve(per_language_suggestions.size() +
                                       browser_suggestions.size());
      per_language_suggestions.insert(per_language_suggestions.end(),
                                      browser_suggestions.begin(),
                                      browser_suggestions.end());
    }
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

    std::vector<base::string16> suggestions;
    spellcheck::FillSuggestions(per_language_suggestions, &suggestions);
    WebVector<WebString> web_suggestions(suggestions.size());
    std::transform(
        suggestions.begin(), suggestions.end(), web_suggestions.begin(),
        [](const base::string16& s) { return WebString::FromUTF16(s); });
    *optional_suggestions = web_suggestions;
    UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.check.suggestions",
                            base::saturated_cast<int>(word.size()));
  } else {
    spellcheck_->SpellCheckWord(word.c_str(), kWordStart, word.size(),
                                routing_id(), &offset, &length,
                                /* optional suggestions vector */ nullptr);
    UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.check",
                            base::saturated_cast<int>(word.size()));
    // If optional_suggestions is not requested, the API is called
    // for marking. So we use this for counting markable words.
    GetSpellCheckHost().NotifyChecked(word, 0 < length);
  }
}

void SpellCheckProvider::RequestCheckingOfText(
    const WebString& text,
    std::unique_ptr<WebTextCheckingCompletion> completion) {
  RequestTextChecking(text.Utf16(), std::move(completion));
  UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.async",
                          base::saturated_cast<int>(text.length()));
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheckProvider::OnRespondSpellingService(
    int identifier,
    const base::string16& line,
    bool success,
    const std::vector<SpellCheckResult>& results) {
  if (!text_check_completions_.Lookup(identifier))
    return;
  std::unique_ptr<WebTextCheckingCompletion> completion(
      text_check_completions_.Replace(identifier, nullptr));
  text_check_completions_.Remove(identifier);

  // If |success| is false, we use local spellcheck as a fallback.
  if (!success) {
    spellcheck_->RequestTextChecking(line, std::move(completion));
    return;
  }

  // Double-check the returned spellchecking results with our spellchecker to
  // visualize the differences between ours and the on-line spellchecker.
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::USE_NATIVE_CHECKER, 0,
                                         line, results, &textcheck_results);
  completion->DidFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.Swap(textcheck_results);
}
#endif

bool SpellCheckProvider::HasWordCharacters(const base::string16& text,
                                           size_t index) const {
  const base::char16* data = text.data();
  size_t length = text.length();
  while (index < length) {
    uint32_t code = 0;
    U16_NEXT(data, index, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON)
      return true;
  }
  return false;
}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckProvider::OnRespondTextCheck(
    int identifier,
    const base::string16& line,
    const std::vector<SpellCheckResult>& results) {
  // TODO(groby): Unify with SpellCheckProvider::OnRespondSpellingService
  DCHECK(spellcheck_);
  if (!text_check_completions_.Lookup(identifier))
    return;
  std::unique_ptr<WebTextCheckingCompletion> completion(
      text_check_completions_.Replace(identifier, nullptr));
  text_check_completions_.Remove(identifier);
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::DO_NOT_MODIFY,
                                         0,
                                         line,
                                         results,
                                         &textcheck_results);
  completion->DidFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.Swap(textcheck_results);
}
#endif

bool SpellCheckProvider::SatisfyRequestFromCache(
    const base::string16& text,
    WebTextCheckingCompletion* completion) {
  size_t last_length = last_request_.length();
  if (!last_length)
    return false;

  // Send back the |last_results_| if the |last_request_| is a substring of
  // |text| and |text| does not have more words to check. Provider cannot cancel
  // the spellcheck request here, because WebKit might have discarded the
  // previous spellcheck results and erased the spelling markers in response to
  // the user editing the text.
  base::string16 request(text);
  size_t text_length = request.length();
  if (text_length >= last_length &&
      !request.compare(0, last_length, last_request_)) {
    if (text_length == last_length || !HasWordCharacters(text, last_length)) {
      completion->DidFinishCheckingText(last_results_);
      return true;
    }
  }

  // Create a subset of the cached results and return it if the given text is a
  // substring of the cached text.
  if (text_length < last_length &&
      !last_request_.compare(0, text_length, request)) {
    size_t result_size = 0;
    for (size_t i = 0; i < last_results_.size(); ++i) {
      size_t start = last_results_[i].location;
      size_t end = start + last_results_[i].length;
      if (start <= text_length && end <= text_length)
        ++result_size;
    }
    blink::WebVector<blink::WebTextCheckingResult> results(last_results_.Data(),
                                                           result_size);
    completion->DidFinishCheckingText(results);
    return true;
  }

  return false;
}

void SpellCheckProvider::OnDestruct() {
  delete this;
}

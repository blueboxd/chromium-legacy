// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spell_check_host_impl.h"

#include "content/public/browser/browser_thread.h"

SpellCheckHostImpl::SpellCheckHostImpl() = default;
SpellCheckHostImpl::~SpellCheckHostImpl() = default;

void SpellCheckHostImpl::RequestDictionary() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  return;
}

void SpellCheckHostImpl::NotifyChecked(const base::string16& word,
                                       bool misspelled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  return;
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheckHostImpl::CallSpellingService(
    const base::string16& text,
    CallSpellingServiceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty())
    mojo::ReportBadMessage("Requested spelling service with empty text");

  // This API requires Chrome-only features.
  std::move(callback).Run(false, std::vector<SpellCheckResult>());
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && !BUILDFLAG(ENABLE_SPELLING_SERVICE)
void SpellCheckHostImpl::RequestTextCheck(const base::string16& text,
                                          int route_id,
                                          RequestTextCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty())
    mojo::ReportBadMessage("Requested text check with empty text");

  session_bridge_.RequestTextCheck(text, std::move(callback));
}

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
void SpellCheckHostImpl::RequestPartialTextCheck(
    const base::string16& text,
    int route_id,
    const std::vector<SpellCheckResult>& partial_results,
    bool fill_suggestions,
    RequestPartialTextCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty())
    mojo::ReportBadMessage("Requested partial text check with empty text");

  // This API requires Chrome-only features.
  std::move(callback).Run(std::vector<SpellCheckResult>());
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

void SpellCheckHostImpl::CheckSpelling(const base::string16& word,
                                       int route_id,
                                       CheckSpellingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED();
  std::move(callback).Run(false);
}

void SpellCheckHostImpl::FillSuggestionList(
    const base::string16& word,
    FillSuggestionListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED();
  std::move(callback).Run({});
}
#endif  //  BUILDFLAG(USE_BROWSER_SPELLCHECKER) &&
        //  !BUILDFLAG(ENABLE_SPELLING_SERVICE)

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
void SpellCheckHostImpl::GetPerLanguageSuggestions(
    const base::string16& word,
    GetPerLanguageSuggestionsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  std::move(callback).Run(std::vector<std::vector<base::string16>>());
}
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

#if defined(OS_ANDROID)
void SpellCheckHostImpl::DisconnectSessionBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_bridge_.DisconnectSession();
}
#endif

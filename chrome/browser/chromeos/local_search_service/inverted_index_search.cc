// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/inverted_index_search.h"

#include <utility>

#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/local_search_service/content_extraction_utils.h"
#include "chrome/browser/chromeos/local_search_service/inverted_index.h"

namespace local_search_service {

namespace {

std::vector<Token> ExtractDocumentTokens(const Data& data,
                                         const std::string& locale) {
  std::vector<Token> document_tokens;
  for (const Content& content : data.contents) {
    const std::vector<Token> content_tokens =
        ExtractContent(content.id, content.content, locale);
    document_tokens.insert(document_tokens.end(), content_tokens.begin(),
                           content_tokens.end());
  }
  return ConsolidateToken(document_tokens);
}

}  // namespace

InvertedIndexSearch::InvertedIndexSearch() {
  inverted_index_ = std::make_unique<InvertedIndex>();
}

InvertedIndexSearch::~InvertedIndexSearch() = default;

uint64_t InvertedIndexSearch::GetSize() {
  return inverted_index_->NumberDocuments();
}

void InvertedIndexSearch::AddOrUpdate(
    const std::vector<local_search_service::Data>& data,
    bool build_index) {
  for (const Data& d : data) {
    // TODO(jiameng): use different locales.
    const std::vector<Token> document_tokens = ExtractDocumentTokens(d, "en");
    DCHECK(!document_tokens.empty());
    inverted_index_->AddDocument(d.id, document_tokens);
  }

  if (build_index) {
    inverted_index_->BuildInvertedIndex();
  }
}

uint32_t InvertedIndexSearch::Delete(const std::vector<std::string>& ids,
                                     bool build_index) {
  uint32_t num_deleted = 0u;
  for (const auto& id : ids) {
    DCHECK(!id.empty());
    num_deleted += inverted_index_->RemoveDocument(id);
  }
  if (build_index) {
    inverted_index_->BuildInvertedIndex();
  }
  return num_deleted;
}

// TODO(jiameng): add impl.
ResponseStatus InvertedIndexSearch::Find(const base::string16& query,
                                         uint32_t max_results,
                                         std::vector<Result>* results) {
  return ResponseStatus::kEmptyIndex;
}

std::vector<std::pair<std::string, uint32_t>>
InvertedIndexSearch::FindTermForTesting(const base::string16& term) const {
  const PostingList posting_list = inverted_index_->FindTerm(term);
  std::vector<std::pair<std::string, uint32_t>> doc_with_freq;
  for (const auto& kv : posting_list) {
    doc_with_freq.push_back({kv.first, kv.second.size()});
  }

  return doc_with_freq;
}

}  // namespace local_search_service

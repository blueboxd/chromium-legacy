// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/inverted_index.h"

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace local_search_service {
TokenPosition::TokenPosition(const std::string& id,
                             uint32_t start_value,
                             uint32_t length_value)
    : content_id(id), start(start_value), length(length_value) {}

Token::Token() = default;
Token::Token(const base::string16& text, const std::vector<TokenPosition>& pos)
    : content(text), positions(pos) {}
Token::Token(const Token& token)
    : content(token.content), positions(token.positions) {}
Token::~Token() = default;

PostingList InvertedIndex::FindTerm(const base::string16& term) const {
  if (dictionary_.find(term) != dictionary_.end())
    return dictionary_.at(term);

  PostingList empty_list;
  return empty_list;
}

InvertedIndex::InvertedIndex() = default;
InvertedIndex::~InvertedIndex() = default;

void InvertedIndex::AddDocument(const std::string& document_id,
                                const std::vector<Token>& tokens) {
  // Removes document if it is already in the inverted index.
  if (doc_length_.find(document_id) != doc_length_.end())
    RemoveDocument(document_id);

  for (const auto& token : tokens) {
    dictionary_[token.content][document_id] = token.positions;
    doc_length_[document_id] += token.positions.size();
  }
}

void InvertedIndex::RemoveDocument(const std::string& document_id) {
  doc_length_.erase(document_id);

  for (auto it = dictionary_.begin(); it != dictionary_.end();) {
    it->second.erase(document_id);

    // Removes term from the dictionary if its posting list is empty.
    if (it->second.empty()) {
      it = dictionary_.erase(it);
    } else {
      it++;
    }
  }
}

std::vector<TfidfResult> InvertedIndex::GetTfidf(const base::string16& term) {
  std::vector<TfidfResult> results;
  const float idf =
      1.0 + log((1.0 + doc_length_.size()) / (1.0 + dictionary_[term].size()));
  for (const auto& item : dictionary_[term]) {
    const float tf =
        static_cast<float>(item.second.size()) / doc_length_[item.first];
    results.push_back({item.first, item.second, tf * idf});
  }
  return results;
}
}  // namespace local_search_service

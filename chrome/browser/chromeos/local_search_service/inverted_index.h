// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_

#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"

namespace local_search_service {
// Stores the content id, start position and length of the token in original
// documents.
struct TokenPosition {
  TokenPosition() = default;
  TokenPosition(const std::string& id,
                uint32_t start_value,
                uint32_t length_value);
  TokenPosition(const TokenPosition& token_position) = default;
  std::string content_id;
  uint32_t start;
  uint32_t length;
};

// Stores the token (after processed). |positions| represents the token's
// positions in one document.
struct Token {
  Token();
  explicit Token(const Token& token);
  Token(const base::string16& text, const std::vector<TokenPosition>& pos);
  ~Token();
  base::string16 content;
  std::vector<TokenPosition> positions;
};

// A posting is a list of TokenPosition.
using Posting = std::vector<TokenPosition>;

// A map from document id to posting.
using PostingList = std::unordered_map<std::string, Posting>;

// A tuple that stores a document ID, token's positions and token's TF-IDF
// score.
using TfidfResult = std::tuple<std::string, Posting, float>;

// InvertedIndex stores the inverted index for local search and provides the
// abilities to add/remove documents, find term, etc.
class InvertedIndex {
 public:
  InvertedIndex();
  ~InvertedIndex();
  InvertedIndex(const InvertedIndex&) = delete;
  InvertedIndex& operator=(const InvertedIndex&) = delete;

  // Returns document ID and positions of a term.
  PostingList FindTerm(const base::string16& term) const;

  // Adds a new document to the inverted index. If the document ID is already in
  // the index, remove the existing and add the new one. All tokens must be
  // unique (have unique content).
  void AddDocument(const std::string& document_id,
                   const std::vector<Token>& tokens);

  // Removes a document from the inverted index. Do nothing if document_id is
  // not in the index.
  void RemoveDocument(const std::string& document_id);

  // Gets TF-IDF scores for a term.
  std::vector<TfidfResult> GetTfidf(const base::string16& term);

 private:
  friend class InvertedIndexTest;

  // Contains the length of the document (the number of terms in the document).
  // The size of this map will always equal to the number of documents in the
  // index.
  std::unordered_map<std::string, int> doc_length_;
  // A map from term to PostingList.
  std::unordered_map<base::string16, PostingList> dictionary_;
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_

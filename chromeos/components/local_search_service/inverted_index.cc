// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/inverted_index.h"

#include <numeric>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/local_search_service/search_utils.h"

namespace chromeos {
namespace local_search_service {

namespace {

// (document-score, posting-of-all-matching-terms).
using ScoreWithPosting = std::pair<double, Posting>;

}  // namespace

PostingList InvertedIndex::FindTerm(const base::string16& term) const {
  if (dictionary_.find(term) != dictionary_.end())
    return dictionary_.at(term);

  return {};
}

std::vector<Result> InvertedIndex::FindMatchingDocumentsApproximately(
    const std::unordered_set<base::string16>& terms,
    double prefix_threshold,
    double block_threshold) const {
  // For each document, its score is the sum of TF-IDF scores of its terms
  // that match one of more query term.
  // The map is keyed by the document id.
  std::unordered_map<std::string, ScoreWithPosting> matching_docs;
  for (const auto& kv : tfidf_cache_) {
    const base::string16& index_term = kv.first;
    const std::vector<TfidfResult>& tfidf_results = kv.second;
    for (const auto& term : terms) {
      if (IsRelevantApproximately(term, index_term, prefix_threshold,
                                  block_threshold)) {
        // If the |index_term| is relevant, all of the enclosing documents will
        // have their ranking scores updated.
        for (const auto& docid_tfidf : tfidf_results) {
          const std::string& docid = std::get<0>(docid_tfidf);
          const Posting& posting = std::get<1>(docid_tfidf);
          const float tfidf = std::get<2>(docid_tfidf);
          auto it = matching_docs.find(docid);
          if (it == matching_docs.end()) {
            it = matching_docs.emplace(docid, ScoreWithPosting(0.0, {})).first;
          }

          auto& score_posting = it->second;
          // TODO(jiameng): add position penalty.
          score_posting.first += tfidf;
          // Also update matching positions.
          auto& existing_posting = score_posting.second;
          existing_posting.insert(existing_posting.end(), posting.begin(),
                                  posting.end());
        }
        // Break out from inner loop, i.e. no need to check other query terms.
        break;
      }
    }
  }

  std::vector<Result> sorted_matching_docs;
  for (const auto& kv : matching_docs) {
    // We don't need to include weights in the search results.
    std::vector<Position> positions;
    for (const auto& weighted_position : kv.second.second) {
      positions.emplace_back(weighted_position.position);
    }
    sorted_matching_docs.emplace_back(
        Result(kv.first, kv.second.first, positions));
  }
  std::sort(sorted_matching_docs.begin(), sorted_matching_docs.end(),
            CompareResults);
  return sorted_matching_docs;
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
    terms_to_be_updated_.insert(token.content);
  }
}

uint32_t InvertedIndex::RemoveDocument(const std::string& document_id) {
  const int num_erased = doc_length_.erase(document_id);

  if (num_erased == 0)
    return num_erased;

  for (auto it = dictionary_.begin(); it != dictionary_.end();) {
    if (it->second.find(document_id) != it->second.end()) {
      terms_to_be_updated_.insert(it->first);
      it->second.erase(document_id);
    }

    // Removes term from the dictionary if its posting list is empty.
    if (it->second.empty()) {
      it = dictionary_.erase(it);
    } else {
      it++;
    }
  }
  return num_erased;
}

std::vector<TfidfResult> InvertedIndex::GetTfidf(
    const base::string16& term) const {
  if (tfidf_cache_.find(term) != tfidf_cache_.end()) {
    return tfidf_cache_.at(term);
  }

  return {};
}

void InvertedIndex::BuildInvertedIndex() {
  // If number of documents doesn't change from the last time index was built,
  // we only need to update terms in |terms_to_be_updated_|. Otherwise we need
  // to rebuild the index.
  if (num_docs_from_last_update_ == doc_length_.size()) {
    for (const auto& term : terms_to_be_updated_) {
      if (dictionary_.find(term) != dictionary_.end()) {
        tfidf_cache_[term] = CalculateTfidf(term);
      } else {
        tfidf_cache_.erase(term);
      }
    }
  } else {
    tfidf_cache_.clear();
    for (const auto& item : dictionary_) {
      tfidf_cache_[item.first] = CalculateTfidf(item.first);
    }
  }

  terms_to_be_updated_.clear();
  num_docs_from_last_update_ = doc_length_.size();
}

std::vector<TfidfResult> InvertedIndex::CalculateTfidf(
    const base::string16& term) {
  std::vector<TfidfResult> results;
  // We don't apply weights to idf because the effect is likely small.
  const float idf =
      1.0 + log((1.0 + doc_length_.size()) / (1.0 + dictionary_[term].size()));

  for (const auto& item : dictionary_[term]) {
    // If a term has a very low content weight in a doc, its effective number of
    // occurrences in the doc should be lower. Strictly speaking, the effective
    // length of the doc should be smaller too. However, for performance
    // reasons, we only apply the weight to the term occurrences but not doc
    // length.
    // TODO(jiameng): this is an expensive operation, we will need to monitor
    // its performance and optimize it.
    const double effective_term_occ = std::accumulate(
        item.second.begin(), item.second.end(), 0.0,
        [](double sum, const WeightedPosition& weighted_position) {
          return sum + weighted_position.weight;
        });
    const float tf = effective_term_occ / doc_length_[item.first];
    results.push_back({item.first, item.second, tf * idf});
  }
  return results;
}

}  // namespace local_search_service
}  // namespace chromeos

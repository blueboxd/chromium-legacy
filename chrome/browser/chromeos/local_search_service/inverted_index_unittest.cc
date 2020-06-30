// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/inverted_index.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/local_search_service/shared_structs.h"
#include "chrome/browser/chromeos/local_search_service/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

namespace {

std::vector<float> GetScoresFromTfidfResult(
    const std::vector<TfidfResult>& results) {
  std::vector<float> scores;
  for (const auto& item : results) {
    scores.push_back(std::roundf(std::get<2>(item) * 100) / 100.0);
  }
  return scores;
}

}  // namespace

class InvertedIndexTest : public ::testing::Test {
 public:
  InvertedIndexTest() = default;
  void SetUp() override {
    index_.doc_length_ =
        std::unordered_map<std::string, int>({{"doc1", 8}, {"doc2", 6}});

    index_.dictionary_[base::UTF8ToUTF16("A")] = PostingList(
        {{"doc1", Posting({Position("header", 1, 1), Position("header", 3, 1),
                           Position("body", 5, 1), Position("body", 7, 1)})},
         {"doc2",
          Posting({Position("header", 2, 1), Position("header", 4, 1)})}});

    index_.dictionary_[base::UTF8ToUTF16("B")] = PostingList(
        {{"doc1",
          Posting({Position("header", 2, 1), Position("body", 4, 1),
                   Position("header", 6, 1), Position("body", 8, 1)})}});

    index_.dictionary_[base::UTF8ToUTF16("C")] = PostingList(
        {{"doc2",
          Posting({Position("header", 1, 1), Position("body", 3, 1),
                   Position("header", 5, 1), Position("body", 7, 1)})}});
    index_.terms_to_be_updated_.insert(base::UTF8ToUTF16("A"));
    index_.terms_to_be_updated_.insert(base::UTF8ToUTF16("B"));
    index_.terms_to_be_updated_.insert(base::UTF8ToUTF16("C"));
  }

  PostingList FindTerm(const base::string16& term) {
    return index_.FindTerm(term);
  }

  std::vector<Result> FindMatchingDocumentsApproximately(
      const std::unordered_set<base::string16>& terms,
      double prefix_threshold,
      double block_threshold) {
    return index_.FindMatchingDocumentsApproximately(terms, prefix_threshold,
                                                     block_threshold);
  }

  void AddDocument(const std::string& doc_id,
                   const std::vector<Token>& tokens) {
    index_.AddDocument(doc_id, tokens);
  }

  void RemoveDocument(const std::string& doc_id) {
    index_.RemoveDocument(doc_id);
  }

  std::vector<TfidfResult> GetTfidf(const base::string16& term) {
    return index_.GetTfidf(term);
  }

  bool GetTfidfForDocId(const base::string16& term,
                        const std::string& docid,
                        float* tfidf,
                        size_t* number_positions) {
    const auto& posting = GetTfidf(term);
    if (posting.empty()) {
      return false;
    }

    for (const auto& tfidf_result : posting) {
      if (std::get<0>(tfidf_result) == docid) {
        *number_positions = std::get<1>(tfidf_result).size();
        *tfidf = std::get<2>(tfidf_result);
        return true;
      }
    }
    return false;
  }

  void BuildInvertedIndex() { index_.BuildInvertedIndex(); }

  bool IsInvertedIndexBuilt() { return index_.IsInvertedIndexBuilt(); }

  std::unordered_map<base::string16, PostingList> GetDictionary() {
    return index_.dictionary_;
  }

  std::unordered_map<std::string, int> GetDocLength() {
    return index_.doc_length_;
  }

  std::unordered_map<base::string16, std::vector<TfidfResult>> GetTfidfCache() {
    return index_.tfidf_cache_;
  }

 private:
  InvertedIndex index_;
};

TEST_F(InvertedIndexTest, FindTermTest) {
  PostingList result = FindTerm(base::UTF8ToUTF16("A"));
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result["doc1"][0].start, 1u);
  EXPECT_EQ(result["doc1"][1].start, 3u);
  EXPECT_EQ(result["doc1"][2].start, 5u);
  EXPECT_EQ(result["doc1"][3].start, 7u);

  EXPECT_EQ(result["doc2"][0].start, 2u);
  EXPECT_EQ(result["doc2"][1].start, 4u);
}

TEST_F(InvertedIndexTest, AddNewDocumentTest) {
  const base::string16 a_utf16(base::UTF8ToUTF16("A"));
  const base::string16 d_utf16(base::UTF8ToUTF16("D"));

  AddDocument("doc3",
              {{a_utf16, {{"header", 1, 1}, {"body", 2, 1}, {"header", 4, 1}}},
               {d_utf16, {{"header", 3, 1}, {"body", 5, 1}}}});

  EXPECT_EQ(GetDocLength()["doc3"], 5);

  // Find "A"
  PostingList result = FindTerm(a_utf16);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result["doc3"][0].start, 1u);
  EXPECT_EQ(result["doc3"][1].start, 2u);
  EXPECT_EQ(result["doc3"][2].start, 4u);

  // Find "D"
  result = FindTerm(d_utf16);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc3"][0].start, 3u);
  EXPECT_EQ(result["doc3"][1].start, 5u);
}

TEST_F(InvertedIndexTest, ReplaceDocumentTest) {
  const base::string16 a_utf16(base::UTF8ToUTF16("A"));
  const base::string16 d_utf16(base::UTF8ToUTF16("D"));

  AddDocument("doc1",
              {{a_utf16, {{"header", 1, 1}, {"body", 2, 1}, {"header", 4, 1}}},
               {d_utf16, {{"header", 3, 1}, {"body", 5, 1}}}});

  EXPECT_EQ(GetDocLength()["doc1"], 5);
  EXPECT_EQ(GetDocLength()["doc2"], 6);

  // Find "A"
  PostingList result = FindTerm(a_utf16);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result["doc1"][0].start, 1u);
  EXPECT_EQ(result["doc1"][1].start, 2u);
  EXPECT_EQ(result["doc1"][2].start, 4u);

  // Find "B"
  result = FindTerm(base::UTF8ToUTF16("B"));
  ASSERT_EQ(result.size(), 0u);

  // Find "D"
  result = FindTerm(d_utf16);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc1"][0].start, 3u);
  EXPECT_EQ(result["doc1"][1].start, 5u);
}

TEST_F(InvertedIndexTest, RemoveDocumentTest) {
  EXPECT_EQ(GetDictionary().size(), 3u);
  EXPECT_EQ(GetDocLength().size(), 2u);

  RemoveDocument("doc1");
  EXPECT_EQ(GetDictionary().size(), 2u);
  EXPECT_EQ(GetDocLength().size(), 1u);
  EXPECT_EQ(GetDocLength()["doc2"], 6);

  // Find "A"
  PostingList result = FindTerm(base::UTF8ToUTF16("A"));
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc2"][0].start, 2u);
  EXPECT_EQ(result["doc2"][1].start, 4u);

  // Find "B"
  result = FindTerm(base::UTF8ToUTF16("B"));
  ASSERT_EQ(result.size(), 0u);

  // Find "C"
  result = FindTerm(base::UTF8ToUTF16("C"));
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc2"][0].start, 1u);
  EXPECT_EQ(result["doc2"][1].start, 3u);
  EXPECT_EQ(result["doc2"][2].start, 5u);
  EXPECT_EQ(result["doc2"][3].start, 7u);
}

TEST_F(InvertedIndexTest, TfidfFromZeroTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  EXPECT_FALSE(IsInvertedIndexBuilt());
  BuildInvertedIndex();

  std::vector<TfidfResult> results = GetTfidf(base::UTF8ToUTF16("A"));
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.5, 0.33));

  results = GetTfidf(base::UTF8ToUTF16("B"));
  EXPECT_EQ(results.size(), 1u);
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.7));

  results = GetTfidf(base::UTF8ToUTF16("C"));
  EXPECT_EQ(results.size(), 1u);
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.94));

  results = GetTfidf(base::UTF8ToUTF16("D"));
  EXPECT_EQ(results.size(), 0u);
}

TEST_F(InvertedIndexTest, UpdateIndexTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  BuildInvertedIndex();
  EXPECT_TRUE(IsInvertedIndexBuilt());
  EXPECT_EQ(GetTfidfCache().size(), 3u);

  // Replaces "doc1"
  AddDocument("doc1",
              {{base::UTF8ToUTF16("A"),
                {{"header", 1, 1}, {"body", 2, 1}, {"header", 4, 1}}},
               {base::UTF8ToUTF16("D"), {{"header", 3, 1}, {"body", 5, 1}}}});

  EXPECT_FALSE(IsInvertedIndexBuilt());
  BuildInvertedIndex();

  EXPECT_EQ(GetTfidfCache().size(), 3u);

  std::vector<TfidfResult> results = GetTfidf(base::UTF8ToUTF16("A"));
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.6, 0.33));

  results = GetTfidf(base::UTF8ToUTF16("B"));
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre());

  results = GetTfidf(base::UTF8ToUTF16("C"));
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.94));

  results = GetTfidf(base::UTF8ToUTF16("D"));
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.56));
}

TEST_F(InvertedIndexTest, FindMatchingDocumentsApproximatelyTest) {
  const double prefix_threshold = 1.0;
  const double block_threshold = 1.0;
  const base::string16 a_utf16(base::UTF8ToUTF16("A"));
  const base::string16 b_utf16(base::UTF8ToUTF16("B"));
  const base::string16 c_utf16(base::UTF8ToUTF16("C"));
  const base::string16 d_utf16(base::UTF8ToUTF16("D"));

  BuildInvertedIndex();

  {
    // "A" exists in "doc1" and "doc2". The score of each document is simply A's
    // TF-IDF score.
    const std::vector<Result> matching_docs =
        FindMatchingDocumentsApproximately({a_utf16}, prefix_threshold,
                                           block_threshold);

    EXPECT_EQ(matching_docs.size(), 2u);
    std::vector<std::string> expected_docids = {"doc1", "doc2"};
    // TODO(jiameng): for testing, we should  use another independent method to
    // verify scores.
    std::vector<float> actual_scores;
    for (size_t i = 0; i < 2; ++i) {
      const auto& docid = expected_docids[i];
      float expected_score = 0;
      size_t expected_num_pos = 0u;
      EXPECT_TRUE(
          GetTfidfForDocId(a_utf16, docid, &expected_score, &expected_num_pos));
      CheckResult(matching_docs[i], docid, expected_score, expected_num_pos);
      actual_scores.push_back(expected_score);
    }

    // Check scores are non-increasing.
    EXPECT_GE(actual_scores[0], actual_scores[1]);
  }

  {
    // "D" does not exist in the index.
    const std::vector<Result> matching_docs =
        FindMatchingDocumentsApproximately({d_utf16}, prefix_threshold,
                                           block_threshold);

    EXPECT_TRUE(matching_docs.empty());
  }

  {
    // Query is {"A", "B", "C"}, which matches all documents.
    const std::vector<Result> matching_docs =
        FindMatchingDocumentsApproximately({a_utf16, b_utf16, c_utf16},
                                           prefix_threshold, block_threshold);
    EXPECT_EQ(matching_docs.size(), 2u);

    // Ranked documents are {"doc2", "doc1"}.
    // "doc2" score comes from sum of TF-IDF of "A" and "C"
    float expected_score2_A = 0;
    size_t expected_num_pos2_A = 0u;
    EXPECT_TRUE(GetTfidfForDocId(a_utf16, "doc2", &expected_score2_A,
                                 &expected_num_pos2_A));
    float expected_score2_C = 0;
    size_t expected_num_pos2_C = 0u;
    EXPECT_TRUE(GetTfidfForDocId(c_utf16, "doc2", &expected_score2_C,
                                 &expected_num_pos2_C));
    const float expected_score2 = expected_score2_A + expected_score2_C;
    const size_t expected_num_pos2 = expected_num_pos2_A + expected_num_pos2_C;
    CheckResult(matching_docs[0], "doc2", expected_score2, expected_num_pos2);

    // "doc1" score comes from sum of TF-IDF of "A" and "B".
    float expected_score1_A = 0;
    size_t expected_num_pos1_A = 0u;
    EXPECT_TRUE(GetTfidfForDocId(a_utf16, "doc1", &expected_score1_A,
                                 &expected_num_pos1_A));
    float expected_score1_B = 0;
    size_t expected_num_pos1_B = 0u;
    EXPECT_TRUE(GetTfidfForDocId(b_utf16, "doc1", &expected_score1_B,
                                 &expected_num_pos1_B));
    const float expected_score1 = expected_score1_A + expected_score1_B;
    const size_t expected_num_pos1 = expected_num_pos1_B + expected_num_pos1_B;
    CheckResult(matching_docs[1], "doc1", expected_score1, expected_num_pos1);

    EXPECT_GE(expected_score2, expected_score1);
  }
}

}  // namespace local_search_service

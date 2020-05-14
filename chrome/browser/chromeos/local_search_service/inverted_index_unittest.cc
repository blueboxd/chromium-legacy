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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

class InvertedIndexTest : public ::testing::Test {
 public:
  InvertedIndexTest() = default;
  void SetUp() override {
    index_.doc_length_ =
        std::unordered_map<std::string, int>({{"doc1", 8}, {"doc2", 6}});

    index_.dictionary_[base::UTF8ToUTF16("A")] = PostingList(
        {{"doc1",
          Posting({TokenPosition("header", 1, 1), TokenPosition("header", 3, 1),
                   TokenPosition("body", 5, 1), TokenPosition("body", 7, 1)})},
         {"doc2", Posting({TokenPosition("header", 2, 1),
                           TokenPosition("header", 4, 1)})}});

    index_.dictionary_[base::UTF8ToUTF16("B")] =
        PostingList({{"doc1", Posting({TokenPosition("header", 2, 1),
                                       TokenPosition("body", 4, 1),
                                       TokenPosition("header", 6, 1),
                                       TokenPosition("body", 8, 1)})}});

    index_.dictionary_[base::UTF8ToUTF16("C")] =
        PostingList({{"doc2", Posting({TokenPosition("header", 1, 1),
                                       TokenPosition("body", 3, 1),
                                       TokenPosition("header", 5, 1),
                                       TokenPosition("body", 7, 1)})}});
    PopulateTfidfCache();
  }

  PostingList FindTerm(const base::string16& term) {
    return index_.FindTerm(term);
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

  void PopulateTfidfCache() { index_.PopulateTfidfCache(); }

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
  ASSERT_EQ(result.size(), static_cast<unsigned long>(2));
  EXPECT_EQ(result["doc1"][0].start, static_cast<uint32_t>(1));
  EXPECT_EQ(result["doc1"][1].start, static_cast<uint32_t>(3));
  EXPECT_EQ(result["doc1"][2].start, static_cast<uint32_t>(5));
  EXPECT_EQ(result["doc1"][3].start, static_cast<uint32_t>(7));

  EXPECT_EQ(result["doc2"][0].start, static_cast<uint32_t>(2));
  EXPECT_EQ(result["doc2"][1].start, static_cast<uint32_t>(4));
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
  ASSERT_EQ(result.size(), static_cast<unsigned long>(3));
  EXPECT_EQ(result["doc3"][0].start, static_cast<uint32_t>(1));
  EXPECT_EQ(result["doc3"][1].start, static_cast<uint32_t>(2));
  EXPECT_EQ(result["doc3"][2].start, static_cast<uint32_t>(4));

  // Find "D"
  result = FindTerm(d_utf16);
  ASSERT_EQ(result.size(), static_cast<unsigned long>(1));
  EXPECT_EQ(result["doc3"][0].start, static_cast<uint32_t>(3));
  EXPECT_EQ(result["doc3"][1].start, static_cast<uint32_t>(5));
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
  ASSERT_EQ(result.size(), static_cast<unsigned long>(2));
  EXPECT_EQ(result["doc1"][0].start, static_cast<uint32_t>(1));
  EXPECT_EQ(result["doc1"][1].start, static_cast<uint32_t>(2));
  EXPECT_EQ(result["doc1"][2].start, static_cast<uint32_t>(4));

  // Find "B"
  result = FindTerm(base::UTF8ToUTF16("B"));
  ASSERT_EQ(result.size(), static_cast<unsigned long>(0));

  // Find "D"
  result = FindTerm(d_utf16);
  ASSERT_EQ(result.size(), static_cast<unsigned long>(1));
  EXPECT_EQ(result["doc1"][0].start, static_cast<uint32_t>(3));
  EXPECT_EQ(result["doc1"][1].start, static_cast<uint32_t>(5));
}

TEST_F(InvertedIndexTest, RemoveDocumentTest) {
  EXPECT_EQ(GetDictionary().size(), static_cast<unsigned long>(3));
  EXPECT_EQ(GetDocLength().size(), static_cast<unsigned long>(2));

  RemoveDocument("doc1");
  EXPECT_EQ(GetDictionary().size(), static_cast<unsigned long>(2));
  EXPECT_EQ(GetDocLength().size(), static_cast<unsigned long>(1));
  EXPECT_EQ(GetDocLength()["doc2"], 6);

  // Find "A"
  PostingList result = FindTerm(base::UTF8ToUTF16("A"));
  ASSERT_EQ(result.size(), static_cast<unsigned long>(1));
  EXPECT_EQ(result["doc2"][0].start, static_cast<uint32_t>(2));
  EXPECT_EQ(result["doc2"][1].start, static_cast<uint32_t>(4));

  // Find "B"
  result = FindTerm(base::UTF8ToUTF16("B"));
  ASSERT_EQ(result.size(), static_cast<unsigned long>(0));

  // Find "C"
  result = FindTerm(base::UTF8ToUTF16("C"));
  ASSERT_EQ(result.size(), static_cast<unsigned long>(1));
  EXPECT_EQ(result["doc2"][0].start, static_cast<uint32_t>(1));
  EXPECT_EQ(result["doc2"][1].start, static_cast<uint32_t>(3));
  EXPECT_EQ(result["doc2"][2].start, static_cast<uint32_t>(5));
  EXPECT_EQ(result["doc2"][3].start, static_cast<uint32_t>(7));
}

TEST_F(InvertedIndexTest, TfidfTest) {
  std::vector<TfidfResult> results = GetTfidf(base::UTF8ToUTF16("A"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(2));
  const std::vector<float> idf_scores = {
      std::roundf(std::get<2>(results[0]) * 100) / 100.0,
      std::roundf(std::get<2>(results[1]) * 100) / 100.0};
  EXPECT_THAT(idf_scores, testing::UnorderedElementsAre(0.5, 0.33));

  results = GetTfidf(base::UTF8ToUTF16("B"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(1));
  EXPECT_NEAR(std::get<2>(results[0]), 0.70, 0.01);

  results = GetTfidf(base::UTF8ToUTF16("C"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(1));
  EXPECT_NEAR(std::get<2>(results[0]), 0.94, 0.01);

  results = GetTfidf(base::UTF8ToUTF16("D"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(0));
}

TEST_F(InvertedIndexTest, PopulateTfidfCacheTest) {
  // Replaces "doc1"
  AddDocument("doc1",
              {{base::UTF8ToUTF16("A"),
                {{"header", 1, 1}, {"body", 2, 1}, {"header", 4, 1}}},
               {base::UTF8ToUTF16("D"), {{"header", 3, 1}, {"body", 5, 1}}}});

  PopulateTfidfCache();

  std::vector<TfidfResult> results = GetTfidf(base::UTF8ToUTF16("A"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(2));
  const std::vector<float> idf_scores = {
      std::roundf(std::get<2>(results[0]) * 100) / 100.0,
      std::roundf(std::get<2>(results[1]) * 100) / 100.0};
  EXPECT_THAT(idf_scores, testing::UnorderedElementsAre(0.6, 0.33));

  results = GetTfidf(base::UTF8ToUTF16("B"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(0));

  results = GetTfidf(base::UTF8ToUTF16("C"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(1));
  EXPECT_NEAR(std::get<2>(results[0]), 0.94, 0.01);

  results = GetTfidf(base::UTF8ToUTF16("D"));
  EXPECT_EQ(results.size(), static_cast<unsigned long>(1));
  EXPECT_NEAR(std::get<2>(results[0]), 0.56, 0.01);
}
}  // namespace local_search_service

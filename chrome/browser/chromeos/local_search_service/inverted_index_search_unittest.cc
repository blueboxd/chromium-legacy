// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/inverted_index_search.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

namespace {

// (content-id, content).
using ContentWithId = std::pair<std::string, std::string>;

// (document-id, number-of-occurrences).
using TermOccurrence = std::vector<std::pair<std::string, uint32_t>>;

}  // namespace

class InvertedIndexSearchTest : public testing::Test {
 protected:
  InvertedIndexSearch search_;
};

TEST_F(InvertedIndexSearchTest, Add) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  search_.AddOrUpdate(data);
  EXPECT_EQ(search_.GetSize(), 2u);

  {
    // "network" does not exist in the index.
    const TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("network"));
    EXPECT_TRUE(doc_with_freq.empty());
  }

  {
    // "help" exists in the index.
    const TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("help"));
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 3u);
    EXPECT_EQ(doc_with_freq[1].first, "id2");
    EXPECT_EQ(doc_with_freq[1].second, 1u);
  }

  {
    // "wifi" exists in the index but "wi-fi" doesn't because of normalization.
    TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("wifi"));
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 2u);
    EXPECT_EQ(doc_with_freq[1].first, "id2");
    EXPECT_EQ(doc_with_freq[1].second, 1u);

    doc_with_freq = search_.FindTermForTesting(base::UTF8ToUTF16("wi-fi"));
    EXPECT_TRUE(doc_with_freq.empty());

    // "WiFi" doesn't exist because the index stores normalized word.
    doc_with_freq = search_.FindTermForTesting(base::UTF8ToUTF16("WiFi"));
    EXPECT_TRUE(doc_with_freq.empty());
  }

  {
    // "this" does not exist in the index because it's a stopword
    const TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("this"));
    EXPECT_TRUE(doc_with_freq.empty());
  }
}

TEST_F(InvertedIndexSearchTest, Update) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  search_.AddOrUpdate(data);
  EXPECT_EQ(search_.GetSize(), 2u);

  const std::map<std::string, std::vector<ContentWithId>> data_to_update = {
      {"id1",
       {{"cid_1", "This is a help bluetooth article"},
        {"cid_2", "Google Playstore Google Music"}}},
      {"id3", {{"cid_3", "Google Map"}}}};

  const std::vector<Data> updated_data = CreateTestData(data_to_update);
  search_.AddOrUpdate(updated_data);
  EXPECT_EQ(search_.GetSize(), 3u);

  {
    const TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("bluetooth"));
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }

  {
    const TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("wifi"));
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id2");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }

  {
    const TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("google"));
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 2u);
    EXPECT_EQ(doc_with_freq[1].first, "id3");
    EXPECT_EQ(doc_with_freq[1].second, 1u);
  }
}

TEST_F(InvertedIndexSearchTest, Delete) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  search_.AddOrUpdate(data);
  EXPECT_EQ(search_.GetSize(), 2u);

  EXPECT_EQ(search_.Delete({"id1", "id3"}), 1u);

  {
    const TermOccurrence doc_with_freq =
        search_.FindTermForTesting(base::UTF8ToUTF16("wifi"));
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id2");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }
}

}  // namespace local_search_service

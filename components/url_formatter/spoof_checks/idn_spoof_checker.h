// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "net/extras/preload_data/decoder.h"

#include "third_party/icu/source/common/unicode/uniset.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/common/unicode/uversion.h"

// 'icu' does not work. Use U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {

class Transliterator;
class UnicodeString;

}  // namespace U_ICU_NAMESPACE

struct USpoofChecker;

namespace url_formatter {
FORWARD_DECLARE_TEST(UrlFormatterTest, IDNToUnicode);

using Skeletons = base::flat_set<std::string>;

// A helper class for IDN Spoof checking, used to ensure that no IDN input is
// spoofable per Chromium's standard of spoofability. For a more thorough
// explanation of how spoof checking works in Chromium, see
// http://dev.chromium.org/developers/design-documents/idn-in-google-chrome .

class IDNSpoofChecker {
 public:
  struct HuffmanTrieParams {
    const uint8_t* huffman_tree;
    size_t huffman_tree_size;
    const uint8_t* trie;
    size_t trie_bits;
    size_t trie_root_position;
  };

  IDNSpoofChecker();
  ~IDNSpoofChecker();

  // Returns true if |label| is safe to display as Unicode. In the event of
  // library failure, all IDN inputs will be treated as unsafe.
  // See the function body for details on the specific safety checks performed.
  bool SafeToDisplayAsUnicode(base::StringPiece16 label, bool is_tld_ascii);

  // Returns the matching top domain if |hostname| or the last few components of
  // |hostname| looks similar to one of top domains listed i
  // top_domains/alexa_domains.list.
  // Two checks are done:
  //   1. Calculate the skeleton of |hostname| based on the Unicode confusable
  //   character list and look it up in the pre-calculated skeleton list of
  //   top domains.
  //   2. Look up the diacritic-free version of |hostname| in the list of
  //   top domains. Note that non-IDN hostnames will not get here.
  std::string GetSimilarTopDomain(base::StringPiece16 hostname);

  // Returns skeleton strings computed from |hostname|. This function can apply
  // extra mappings to some characters to produce multiple skeletons.
  Skeletons GetSkeletons(base::StringPiece16 hostname);

  // Returns a top domain from the top 10K list matching the given |skeleton|.
  std::string LookupSkeletonInTopDomains(const std::string& skeleton);

 private:
  // Sets allowed characters in IDN labels and turns on USPOOF_CHAR_LIMIT.
  void SetAllowedUnicodeSet(UErrorCode* status);

  // Returns true if all the Cyrillic letters in |label| belong to a set of
  // Cyrillic letters that look like ASCII Latin letters.
  bool IsMadeOfLatinAlikeCyrillic(const icu::UnicodeString& label);

  // Used for unit tests.
  static void SetTrieParamsForTesting(const HuffmanTrieParams& trie_params);
  static void RestoreTrieParamsForTesting();

  USpoofChecker* checker_;
  icu::UnicodeSet deviation_characters_;
  icu::UnicodeSet non_ascii_latin_letters_;
  icu::UnicodeSet kana_letters_exceptions_;
  icu::UnicodeSet combining_diacritics_exceptions_;
  icu::UnicodeSet cyrillic_letters_;
  icu::UnicodeSet cyrillic_letters_latin_alike_;
  icu::UnicodeSet lgc_letters_n_ascii_;
  std::unique_ptr<icu::Transliterator> diacritic_remover_;
  std::unique_ptr<icu::Transliterator> extra_confusable_mapper_;

  FRIEND_TEST_ALL_PREFIXES(IDNSpoofCheckerTest, IDNToUnicode);
  IDNSpoofChecker(const IDNSpoofChecker&) = delete;
  void operator=(const IDNSpoofChecker&) = delete;
};

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_

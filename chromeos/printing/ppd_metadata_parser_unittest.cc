// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_metadata_parser.h"

#include "base/strings/string_piece.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using ::testing::ElementsAre;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

constexpr base::StringPiece kInvalidJson = "blah blah invalid JSON";

// Matches a ReverseIndexLeaf struct against its |manufacturer| and
// |model| members.
MATCHER_P2(ReverseIndexLeafLike,
           manufacturer,
           model,
           "is a ReverseIndexLeaf with manufacturer ``" +
               std::string(manufacturer) + "'' and model ``" +
               std::string(model) + "''") {
  return ExplainMatchResult(
             Field(&ReverseIndexLeaf::manufacturer, StrEq(manufacturer)), arg,
             result_listener) &&
         ExplainMatchResult(Field(&ReverseIndexLeaf::model, StrEq(model)), arg,
                            result_listener);
}

// Matches a ParsedPrinter struct against its
// |user_visible_printer_name| and |effective_make_and_model| members.
MATCHER_P2(ParsedPrinterLike,
           name,
           emm,
           "is a ParsedPrinter with user_visible_printer_name``" +
               std::string(name) + "'' and effective_make_and_model ``" +
               std::string(emm) + "''") {
  return ExplainMatchResult(
             Field(&ParsedPrinter::user_visible_printer_name, StrEq(name)), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&ParsedPrinter::effective_make_and_model, StrEq(emm)), arg,
             result_listener);
}

// Verifies that ParseLocales() can parse locales metadata.
TEST(PpdMetadataParserTest, CanParseLocales) {
  constexpr base::StringPiece kLocalesJson = R"(
  {
    "locales": [ "de", "en", "es", "jp" ]
  }
  )";

  const auto parsed = ParseLocales(kLocalesJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              ElementsAre(StrEq("de"), StrEq("en"), StrEq("es"), StrEq("jp")));
}

// Verifies that ParseLocales() can parse locales and return a partial
// list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParseLocales) {
  // The values "0.0" and "78" are gibberish that ParseLocales() shall
  // ignore; however, these don't structurally foul the JSON, so it can
  // still return the other locales.
  constexpr base::StringPiece kLocalesJson = R"(
  {
    "locales": [ 0.0, "de", 78, "en", "es", "jp" ]
  }
  )";

  const auto parsed = ParseLocales(kLocalesJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              ElementsAre(StrEq("de"), StrEq("en"), StrEq("es"), StrEq("jp")));
}

// Verifies that ParseLocales() returns base::nullopt rather than an
// empty container.
TEST(PpdMetadataParserTest, ParseLocalesDoesNotReturnEmptyContainer) {
  // The values "0.0" and "78" are gibberish that ParseLocales() shall
  // ignore; while the JSON is still well-formed, the parsed list of
  // locales contains no values.
  constexpr base::StringPiece kLocalesJson = R"(
  {
    "locales": [ 0.0, 78 ]
  }
  )";

  EXPECT_FALSE(ParseLocales(kLocalesJson).has_value());
}

// Verifies that ParseLocales() returns base::nullopt on irrecoverable
// parse error.
TEST(PpdMetadataParserTest, ParseLocalesFailsGracefully) {
  EXPECT_FALSE(ParseLocales(kInvalidJson).has_value());
}

// Verifies that ParseManufacturers() can parse manufacturers metadata.
TEST(PpdMetadataParserTest, CanParseManufacturers) {
  constexpr base::StringPiece kManufacturersJson = R"(
  {
    "filesMap": {
      "Andante": "andante-en.json",
      "Sostenuto": "sostenuto-en.json"
    }
  }
  )";

  const auto parsed = ParseManufacturers(kManufacturersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              UnorderedElementsAre(
                  Pair(StrEq("Andante"), StrEq("andante-en.json")),
                  Pair(StrEq("Sostenuto"), StrEq("sostenuto-en.json"))));
}

// Verifies that ParseManufacturers() can parse manufacturers and return
// a partial list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParseManufacturers) {
  // Contains an embedded dictionary keyed on "Dearie me."
  // ParseManufacturers() shall ignore this.
  constexpr base::StringPiece kManufacturersJson = R"(
  {
    "filesMap": {
      "Dearie me": {
        "I didn't": "expect",
        "to go": "deeper"
      },
      "Andante": "andante-en.json",
      "Sostenuto": "sostenuto-en.json"
    }
  }
  )";

  const auto parsed = ParseManufacturers(kManufacturersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              UnorderedElementsAre(
                  Pair(StrEq("Andante"), StrEq("andante-en.json")),
                  Pair(StrEq("Sostenuto"), StrEq("sostenuto-en.json"))));
}

// Verifies that ParseManufacturers() returns base::nullopt rather than
// an empty container.
TEST(PpdMetadataParserTest, ParseManufacturersDoesNotReturnEmptyContainer) {
  // Contains an embedded dictionary keyed on "Dearie me."
  // ParseManufacturers() shall ignore this, but in doing so shall leave
  // its ParsedManufacturers return value empty.
  constexpr base::StringPiece kManufacturersJson = R"(
  {
    "filesMap": {
      "Dearie me": {
        "I didn't": "expect",
        "to go": "deeper"
      }
    }
  }
  )";

  EXPECT_FALSE(ParseManufacturers(kManufacturersJson).has_value());
}

// Verifies that ParseManufacturers() returns base::nullopt on
// irrecoverable parse error.
TEST(PpdMetadataParserTest, ParseManufacturersFailsGracefully) {
  EXPECT_FALSE(ParseManufacturers(kInvalidJson).has_value());
}

// Verifies that ParsePrinters() can parse printers metadata.
TEST(PpdMetadataParserTest, CanParsePrinters) {
  constexpr base::StringPiece kPrintersJson = R"(
  {
    "modelToEmm": {
      "An die Musik": "d 547b",
      "Auf der Donau": "d 553"
    }
  }
  )";

  const auto parsed = ParsePrinters(kPrintersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed, UnorderedElementsAre(
                           ParsedPrinterLike("An die Musik", "d 547b"),
                           ParsedPrinterLike("Auf der Donau", "d 553")));
}

// Verifies that ParsePrinters() can parse printers and return a partial
// list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParsePrinters) {
  // Contains an embedded dictionary keyed on "Dearie me."
  // ParsePrinters() shall ignore this.
  constexpr base::StringPiece kPrintersJson = R"(
  {
    "modelToEmm": {
      "Dearie me": {
        "I didn't": "expect",
        "to go": "deeper"
      },
      "Hänflings Liebeswerbung": "d 552",
      "Auf der Donau": "d 553"
    }
  }
  )";

  const auto parsed = ParsePrinters(kPrintersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              UnorderedElementsAre(
                  ParsedPrinterLike("Hänflings Liebeswerbung", "d 552"),
                  ParsedPrinterLike("Auf der Donau", "d 553")));
}

// Verifies that ParsePrinters() returns base::nullopt rather than an
// empty container.
TEST(PpdMetadataParserTest, ParsePrintersDoesNotReturnEmptyContainer) {
  // Contains an embedded dictionary keyed on "Dearie me."
  // ParsePrinters() shall ignore this, but in doing so shall make the
  // returned ParsedPrinters empty.
  constexpr base::StringPiece kPrintersJson = R"(
  {
    "modelToEmm": {
      "Dearie me": {
        "I didn't": "expect",
        "to go": "deeper"
      }
    }
  }
  )";

  EXPECT_FALSE(ParsePrinters(kPrintersJson).has_value());
}

// Verifies that ParsePrinters() returns base::nullopt on irrecoverable
// parse error.
TEST(PpdMetadataParserTest, ParsePrintersFailsGracefully) {
  EXPECT_FALSE(ParsePrinters(kInvalidJson).has_value());
}

// Verifies that ParseReverseIndex() can parse reverse index metadata.
TEST(PpdMetadataParserTest, CanParseReverseIndex) {
  constexpr base::StringPiece kReverseIndexJson = R"(
  {
    "reverseIndex": {
      "Die Forelle D 550d": {
        "manufacturer": "metsukabi",
        "model": "kimebe"
      },
      "Gruppe aus dem Tartarus D 583": {
        "manufacturer": "teiga",
        "model": "dahuho"
      }
    }
  }
  )";

  const auto parsed = ParseReverseIndex(kReverseIndexJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed, UnorderedElementsAre(
                           Pair(StrEq("Die Forelle D 550d"),
                                ReverseIndexLeafLike("metsukabi", "kimebe")),
                           Pair(StrEq("Gruppe aus dem Tartarus D 583"),
                                ReverseIndexLeafLike("teiga", "dahuho"))));
}

// Verifies that ParseReverseIndex() can parse reverse index metadata
// and return a partial list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParseReverseIndex) {
  // Contains two unexpected values (keyed on "Dearie me" and "to go").
  // ParseReverseIndex() shall ignore these.
  constexpr base::StringPiece kReverseIndexJson = R"(
  {
    "reverseIndex": {
      "Dearie me": "one doesn't expect",
      "to go": "any deeper",
      "Elysium D 584": {
        "manufacturer": "nahopenu",
        "model": "sapudo"
      },
      "An den Tod D 518": {
        "manufacturer": "suwaka",
        "model": "zogegi"
      }
    }
  }
  )";

  const auto parsed = ParseReverseIndex(kReverseIndexJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed, UnorderedElementsAre(
                           Pair(StrEq("Elysium D 584"),
                                ReverseIndexLeafLike("nahopenu", "sapudo")),
                           Pair(StrEq("An den Tod D 518"),
                                ReverseIndexLeafLike("suwaka", "zogegi"))));
}

// Verifies that ParseReverseIndex() returns base::nullopt rather than
// an empty container.
TEST(PpdMetadataParserTest, ParseReverseIndexDoesNotReturnEmptyContainer) {
  // Contains two unexpected values (keyed on "Dearie me" and "to go").
  // ParseReverseIndex() shall ignore this, but in doing so shall make the
  // returned ParsedReverseIndex empty.
  constexpr base::StringPiece kReverseIndexJson = R"(
  {
    "reverseIndex": {
      "Dearie me": "one doesn't expect",
      "to go": "any deeper"
    }
  }
  )";

  EXPECT_FALSE(ParseReverseIndex(kReverseIndexJson).has_value());
}

// Verifies that ParseReverseIndex() returns base::nullopt on
// irrecoverable parse error.
TEST(PpdMetadataParserTest, ParseReverseIndexFailsGracefully) {
  EXPECT_FALSE(ParseReverseIndex(kInvalidJson).has_value());
}

}  // namespace
}  // namespace chromeos

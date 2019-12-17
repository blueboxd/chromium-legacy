// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;

constexpr char kQueryTermPath[] = "dictionaryResult.queryTerm";
constexpr char kDictionaryEntriesPath[] = "dictionaryResult.entries";
constexpr char kSenseFamiliesKey[] = "senseFamilies";
constexpr char kSensesKey[] = "senses";
constexpr char kDefinitionPathUnderSense[] = "definition.text";
constexpr char kPhoneticsKey[] = "phonetics";
constexpr char kPhoneticsTextKey[] = "text";
constexpr char kPhoneticsResultTemplate[] = "%s · /%s/";

}  // namespace

bool DefinitionResultParser::Parse(const Value* result,
                                   QuickAnswer* quick_answer) {
  const Value* first_entry =
      GetFirstListElement(*result, kDictionaryEntriesPath);
  if (!first_entry) {
    LOG(ERROR) << "Can't find a definition entry.";
    return false;
  }

  // Get Definition.
  const std::string* definition = ExtractDefinition(first_entry);
  if (!definition) {
    LOG(ERROR) << "Fail in extracting definition";
    return false;
  }

  const std::string* phonetics = ExtractPhonetics(first_entry);
  if (!phonetics) {
    LOG(ERROR) << "Fail in extracting phonetics";
    return false;
  }

  const std::string* query_term = result->FindStringPath(kQueryTermPath);
  if (!query_term) {
    LOG(ERROR) << "Fail in extracting query term";
    return false;
  }

  quick_answer->result_type = ResultType::kDefinitionResult;
  quick_answer->primary_answer = *definition;
  quick_answer->secondary_answer = base::StringPrintf(
      kPhoneticsResultTemplate, query_term->c_str(), phonetics->c_str());
  return true;
}

const std::string* DefinitionResultParser::ExtractDefinition(
    const base::Value* definition_entry) {
  const Value* first_sense_family =
      GetFirstListElement(*definition_entry, kSenseFamiliesKey);
  if (!first_sense_family) {
    LOG(ERROR) << "Can't find a sense family.";
    return nullptr;
  }

  const Value* first_sense =
      GetFirstListElement(*first_sense_family, kSensesKey);
  if (!first_sense) {
    LOG(ERROR) << "Can't find a sense.";
    return nullptr;
  }

  return first_sense->FindStringPath(kDefinitionPathUnderSense);
}

const std::string* DefinitionResultParser::ExtractPhonetics(
    const base::Value* definition_entry) {
  const Value* first_phonetics =
      GetFirstListElement(*definition_entry, kPhoneticsKey);
  if (!first_phonetics) {
    LOG(ERROR) << "Can't find a phonetics.";
    return nullptr;
  }

  return first_phonetics->FindStringPath(kPhoneticsTextKey);
}

}  // namespace quick_answers
}  // namespace chromeos
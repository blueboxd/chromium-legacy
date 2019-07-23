// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/mac/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/autofill/automation/automation_action.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/js_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
static const char kAutofillAutomationSwitch[] = "autofillautomation";
static const int kRecipeRetryLimit = 5;
}

// The autofill automation test case is intended to run a script against a
// captured web site. It gets the script from the command line.
@interface AutofillAutomationTestCase : ChromeTestCase {
  bool shouldRecordException;
  GURL startUrl;
  NSMutableArray<AutomationAction*>* actions_;
  std::map<const std::string, autofill::ServerFieldType>
      string_to_field_type_map_;
}
@end

@implementation AutofillAutomationTestCase

// Retrieves the path to the recipe file from the command line.
+ (const base::FilePath)recipePath {
  base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());

  GREYAssert(command_line->HasSwitch(kAutofillAutomationSwitch),
             @"Missing command line switch %s.", kAutofillAutomationSwitch);

  base::FilePath path(
      command_line->GetSwitchValuePath(kAutofillAutomationSwitch));

  GREYAssert(!path.empty(),
             @"A file name must be specified for command line switch %s.",
             kAutofillAutomationSwitch);

  GREYAssert(path.IsAbsolute(),
             @"A fully qualified file name must be specified for command "
             @"line switch %s.",
             kAutofillAutomationSwitch);

  GREYAssert(base::PathExists(path), @"File not found for switch %s.",
             kAutofillAutomationSwitch);
  return path;
}

// Loads the recipe file, parse it into Values.
+ (base::Value)parseRecipeAtPath:(const base::FilePath&)path {
  std::string json_text;

  bool readSuccess(base::ReadFileToString(path, &json_text));
  GREYAssert(readSuccess, @"Unable to read json file.");

  base::Optional<base::Value> value = base::JSONReader::Read(json_text);

  GREYAssert(value.has_value(), @"Unable to parse json file.");
  GREYAssert(value.value().is_dict(),
             @"Expecting a dictionary in the JSON file.");

  return std::move(value).value();
}

// Converts a string (from the test recipe) to the autofill ServerFieldType it
// represents.
- (autofill::ServerFieldType)serverFieldTypeFromString:(const std::string&)str {
  // Lazily init the string to autofill field type map on the first call.
  // The test recipe can contain both server and html field types, as when
  // creating the recipe either type can be returned from predictions.
  // Therefore, we store both in this map.
  if (string_to_field_type_map_.empty()) {
    for (size_t i = autofill::NO_SERVER_DATA;
         i < autofill::MAX_VALID_FIELD_TYPE; ++i) {
      autofill::ServerFieldType field_type =
          static_cast<autofill::ServerFieldType>(i);
      string_to_field_type_map_[autofill::AutofillType(field_type).ToString()] =
          field_type;
    }

    for (size_t i = autofill::HTML_TYPE_UNSPECIFIED;
         i < autofill::HTML_TYPE_UNRECOGNIZED; ++i) {
      autofill::AutofillType field_type(static_cast<autofill::HtmlFieldType>(i),
                                        autofill::HTML_MODE_NONE);
      string_to_field_type_map_[field_type.ToString()] =
          field_type.GetStorableType();
    }
  }

  if (string_to_field_type_map_.count(str) == 0) {
    NSString* errorStr = [NSString
        stringWithFormat:@"Unable to recognize autofill field type %@!",
                         base::SysUTF8ToNSString(str)];
    GREYAssert(false, errorStr);
  }

  return string_to_field_type_map_[str];
}

// Loads the defined autofill profile into the personal data manager, so that
// autofill actions will be suggested when tapping on an autofillable form.
// The autofill profile should be pulled from the test recipe, and consists of
// a list of dictionaries, each mapping one autofill type to one value, like so:
// "autofillProfile": [
//   { "type": "NAME_FIRST", "value": "Satsuki" },
//   { "type": "NAME_LAST", "value": "Yumizuka" },
//  ],
- (void)prepareAutofillProfileWithValues:(const base::Value*)autofillProfile {
  autofill::AutofillProfile profile(base::GenerateGUID(),
                                    "https://www.example.com/");
  autofill::CreditCard credit_card(base::GenerateGUID(),
                                   "https://www.example.com/");

  const base::Value::ListStorage& profile_entries_list =
      autofillProfile->GetList();

  // For each type-value dictionary in the autofill profile list, validate it,
  // then add it to the appropriate profile.
  for (base::ListValue::const_iterator it_entry = profile_entries_list.begin();
       it_entry != profile_entries_list.end(); ++it_entry) {
    const base::DictionaryValue* entry;

    GREYAssert(it_entry->GetAsDictionary(&entry),
               @"Failed to extract an entry!");

    const base::Value* type_container = entry->FindKey("type");
    GREYAssert(base::Value::Type::STRING == type_container->type(),
               @"Type is not a string!");
    const std::string field_type = type_container->GetString();

    const base::Value* value_container = entry->FindKey("value");
    GREYAssert(base::Value::Type::STRING == value_container->type(),
               @"Value is not a string!");
    const std::string field_value = value_container->GetString();

    autofill::ServerFieldType type =
        [self serverFieldTypeFromString:field_type];

    // TODO(crbug.com/895968): Autofill profile and credit card info should be
    // loaded from separate fields in the recipe, instead of being grouped
    // together. However, we need to make sure this change is also performed on
    // desktop automation.
    if (base::StartsWith(field_type, "HTML_TYPE_CREDIT_CARD_",
                         base::CompareCase::INSENSITIVE_ASCII) ||
        base::StartsWith(field_type, "CREDIT_CARD_",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      credit_card.SetRawInfo(type, base::UTF8ToUTF16(field_value));
    } else {
      profile.SetRawInfo(type, base::UTF8ToUTF16(field_value));
    }
  }

  // Save the profile and credit card generated to the personal data manager.
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  web::WebFrame* main_frame =
      web_state->GetWebFramesManager()->GetMainWebFrame();
  autofill::AutofillManager* autofill_manager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state,
                                                           main_frame)
          ->autofill_manager();
  autofill::PersonalDataManager* personal_data_manager =
      autofill_manager->client()->GetPersonalDataManager();

  personal_data_manager->ClearAllLocalData();
  personal_data_manager->AddCreditCard(credit_card);
  personal_data_manager->SaveImportedProfile(profile);
}

- (void)setUp {
  [super setUp];

  const base::FilePath recipePath = [[self class] recipePath];
  base::Value recipeRoot = [[self class] parseRecipeAtPath:recipePath];

  const base::Value* autofillProfile =
      recipeRoot.FindKeyOfType("autofillProfile", base::Value::Type::LIST);
  if (autofillProfile) {
    [self prepareAutofillProfileWithValues:autofillProfile];
  }

  // Extract the starting URL.
  base::Value* startUrlValue =
      recipeRoot.FindKeyOfType("startingURL", base::Value::Type::STRING);
  GREYAssert(startUrlValue, @"Test file is missing startingURL.");

  const std::string startUrlString(startUrlValue->GetString());
  GREYAssert(!startUrlString.empty(), @"startingURL is an empty value.");

  startUrl = GURL(startUrlString);

  // Extract the actions.
  base::Value* actionValue =
      recipeRoot.FindKeyOfType("actions", base::Value::Type::LIST);
  GREYAssert(actionValue, @"Test file is missing actions.");

  const base::Value::ListStorage& actionsValues(actionValue->GetList());
  GREYAssert(actionsValues.size(), @"Test file has empty actions.");

  actions_ = [[NSMutableArray alloc] initWithCapacity:actionsValues.size()];
  for (auto const& actionValue : actionsValues) {
    GREYAssert(actionValue.is_dict(),
               @"Expecting each action to be a dictionary in the JSON file.");
    [actions_ addObject:[AutomationAction
                            actionWithValueDictionary:
                                static_cast<const base::DictionaryValue&>(
                                    actionValue)]];
  }
}

// Override the XCTestCase method that records a failure due to an exception.
// This way we can choose whether to report failures during multiple runs of
// a recipe, and only fail the test if all the runs of the recipe fail.
// We still print the failure even when it is not reported.
- (void)recordFailureWithDescription:(NSString*)description
                              inFile:(NSString*)filePath
                              atLine:(NSUInteger)lineNumber
                            expected:(BOOL)expected {
  if (self->shouldRecordException) {
    [super recordFailureWithDescription:description
                                 inFile:filePath
                                 atLine:lineNumber
                               expected:expected];
  } else {
    NSLog(@"%@", description);
  }
}

// Runs the recipe provided multiple times.
// If any of the runs succeed, the test will be reported as a success.
- (void)testActions {
  for (int i = 0; i < kRecipeRetryLimit; i++) {
    // Only actually report the exception on the last run.
    // This is because any exception reporting will fail the test.
    NSLog(@"================================================================");
    NSLog(@"RECIPE ATTEMPT %d of %d for %@", (i + 1), kRecipeRetryLimit,
          base::SysUTF8ToNSString(startUrl.GetContent()));

    self->shouldRecordException = (i == (kRecipeRetryLimit - 1));

    if ([self runActionsOnce]) {
      return;
    }
  }
}

// Tries running the recipe against the target website once.
// Returns true if the entire recipe succeeds.
// Returns false if an assertion is raised due to a failure.
- (bool)runActionsOnce {
  @try {
    // Load the initial page of the recipe.
    [ChromeEarlGrey loadURL:startUrl];

    for (AutomationAction* action in actions_) {
      [action execute];
    }
  } @catch (NSException* e) {
    return false;
  }

  return true;
}

@end

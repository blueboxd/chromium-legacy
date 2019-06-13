// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/automation/automation_action.h"

#import <EarlGrey/EarlGrey.h>

#include "base/guid.h"
#include "base/mac/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/nserror_util.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/js_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutomationAction () {
  std::unique_ptr<const base::DictionaryValue> actionDictionary_;
}

@property(nonatomic, readonly)
    const std::unique_ptr<const base::DictionaryValue>& actionDictionary;

// Selects the proper subclass in the class cluster for the given type. Called
// from the class method creating the actions.
+ (Class)classForType:(NSString*)type;

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary NS_DESIGNATED_INITIALIZER;
@end

// An action that always fails.
@interface AutomationActionUnrecognized : AutomationAction
@end

// An action that simply tap on an element on the page.
// Right now this always assumes a click event of the following format:
//  {
//    "selectorType": "xpath",
//    "selector": "//*[@id=\"add-to-cart-button\"]",
//    "context": [],
//    "type": "click"
//  }
@interface AutomationActionClick : AutomationAction
@end

// An action that waits for a series of JS assertions to become true before
// continuing. We assume this action has a format resembling:
// {
//   "context": [],
//   "type": "waitFor",
//   "assertions": ["return document.querySelector().style.display ===
//   'none';"]
// }
@interface AutomationActionWaitFor : AutomationAction
@end

// An action that performs autofill on a form by selecting an element
// that is part of an autofillable form, then tapping the relevant
// autofill suggestion. We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@data-tl-id=\"COAC2ShpAddrFirstName\"]",
//   "context": [],
//   "type": "autofill"
// }
@interface AutomationActionAutofill : AutomationAction
@end

// An action that validates a previously autofilled element.
// Confirms that the target element has is of the expected autofill field type
// and contains the specified value.
// We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@data-tl-id=\"COAC2ShpAddrFirstName\"]",
//   "context": [],
//   "expectedAutofillType": "NAME_FIRST",
//   "expectedValue": "Yuki",
//   "type": "validateField"
// }
@interface AutomationActionValidateField : AutomationAction
@end

// An action that selects a given option from a dropdown selector.
// Checks are not made to confirm that given item is a dropdown.
// We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@id=\"shipping-user-lookup-options\"]",
//   "context": [],
//   "index": 1,
//   "type": "select"
// }
@interface AutomationActionSelectDropdown : AutomationAction
@end

// An action that loads a web page.
// This is recorded in tandem with the actions that cause loads to
// occur (i.e. clicking on a link); therefore, this action is
// a no-op when replaying.
// We assume this action has a format resembling:
// {
//   "url": "www.google.com",
//   "type": "loadPage"
// }
@interface AutomationActionLoadPage : AutomationAction
@end

// An action that types the provided text in the specified field.
// This can be either of the "type" or "typePassword" type due to
// the two actions needing to be handled differently on desktop in order
// to ensure passwords are saved. They can be treated the same on iOS.
// We assume this action has a format resembling:
// {
//   "type": "type" OR "typePassword",
//   "selector": "//input[@autocapitalize=\"none\" and
//   @name=\"session[password]\"]", "value": "mycoolpassword",
// }
@interface AutomationActionType : AutomationAction
@end

// An action that selects the affirmative option in an open confirmation dialog.
// One main use case is selecting "Save" from the "Save password?" dialog,
// but this action cannot tell the difference between different confirmation
// dialogs, so it is multi-purpose. This action assumes that this dialog is
// already open.
// We assume this action has a format resembling:
// {
//   "type": "savePassword"
// }
@interface AutomationActionConfirmInfobar : AutomationAction
@end

@implementation AutomationAction

+ (instancetype)actionWithValueDictionary:
                    (const base::DictionaryValue&)actionDictionary
                                    error:(NSError**)error {
  const base::Value* typeValue =
      actionDictionary.FindKeyOfType("type", base::Value::Type::STRING);
  if (!typeValue) {
    *error =
        testing::NSErrorWithLocalizedDescription(@"Type is missing in action.");
    return nil;
  }

  const std::string type(typeValue->GetString());
  if (type.empty()) {
    *error =
        testing::NSErrorWithLocalizedDescription(@"Type is an empty value.");
    return nil;
  }

  return [[[self classForType:base::SysUTF8ToNSString(type)] alloc]
      initWithValueDictionary:actionDictionary];
}

+ (Class)classForType:(NSString*)type {
  static NSDictionary* classForType = @{
    @"click" : [AutomationActionClick class],
    @"waitFor" : [AutomationActionWaitFor class],
    @"autofill" : [AutomationActionAutofill class],
    @"validateField" : [AutomationActionValidateField class],
    @"select" : [AutomationActionSelectDropdown class],
    @"loadPage" : [AutomationActionLoadPage class],
    @"type" : [AutomationActionType class],
    @"typePassword" : [AutomationActionType class],
    @"savePassword" : [AutomationActionConfirmInfobar class],
    // More to come.
  };

  return classForType[type] ?: [AutomationActionUnrecognized class];
}

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary {
  self = [super init];
  if (self) {
    actionDictionary_ = actionDictionary.DeepCopyWithoutEmptyChildren();
  }
  return self;
}

- (NSError*)execute {
  return testing::NSErrorWithLocalizedDescription(
      @"Default AutomationAction::execute should not be called!");
}

- (const std::unique_ptr<const base::DictionaryValue>&)actionDictionary {
  return actionDictionary_;
}

// A shared flow across many actions, this waits for the target element to be
// visible, scrolls it into view, then taps on it.
- (void)tapOnTarget:(ElementSelector*)selector {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebStateContainingElement:selector];

  // Potentially scroll into view if below the fold.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:WebViewScrollElementToVisible(web_state, selector)];

  // Calling WebViewTapElement right after WebViewScrollElement caused flaky
  // issues with the wrong location being provided for the tap target,
  // seemingly caused by the screen not redrawing in-between these two actions.
  // We force a brief wait here to avoid this issue.
  [[GREYCondition conditionWithName:@"forced wait to allow for redraw"
                              block:^BOOL {
                                return false;
                              }] waitWithTimeout:0.1];

  // Tap on the element.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:web::WebViewTapElement(web_state, selector)];
}

// Creates a selector targeting the element specified in the action.
- (ElementSelector*)selectorForTarget:(NSError**)error {
  const std::string xpath = [self getStringFromDictionaryWithKey:"selector"
                                                           error:error];
  if (*error) {
    return nil;
  }

  // Creates a selector from the action dictionary.
  ElementSelector* selector = [ElementSelector selectorWithXPathQuery:xpath];
  return selector;
}

// Returns a std::string corrensponding to the given key in the action
// dictionary. Will raise a test failure if the key is missing or the value is
// empty.
- (std::string)getStringFromDictionaryWithKey:(std::string)key
                                        error:(NSError**)error {
  const base::Value* expectedTypeValue(
      self.actionDictionary->FindKeyOfType(key, base::Value::Type::STRING));
  if (!expectedTypeValue) {
    *error = testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"%s is missing in action.", key.c_str()]);
    return "";
  }

  const std::string expectedType(expectedTypeValue->GetString());
  if (expectedType.empty()) {
    *error = testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"%s is an empty value", key.c_str()]);
    return "";
  }

  return expectedType;
}

// Returns an int corrensponding to the given key in the action
// dictionary. Will raise a test failure if the key is missing or the value is
// empty.
- (int)getIntFromDictionaryWithKey:(std::string)key error:(NSError**)error {
  const base::Value* expectedTypeValue(
      self.actionDictionary->FindKeyOfType(key, base::Value::Type::INTEGER));
  if (!expectedTypeValue) {
    *error = testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"%s is missing in action.", key.c_str()]);
    return 0;
  }

  return expectedTypeValue->GetInt();
}

// Runs the JS code passed in against the target element specified by the
// selector passed in. The target element is passed in to the JS function
// by the name "target", so example JS code is like:
// return target.value
- (id)executeJavascript:(std::string)function
               onTarget:(ElementSelector*)selector
                  error:(NSError**)error {
  id result = chrome_test_util::ExecuteJavaScript(
      [NSString
          stringWithFormat:@"    (function() {"
                            "      try {"
                            "        return function(target){%@}(%@);"
                            "      } catch (ex) {return 'Exception encountered "
                            "' + ex.message;}"
                            "     "
                            "    })();",
                           base::SysUTF8ToNSString(function),
                           selector.selectorScript],
      error);

  return result;
}

@end

@implementation AutomationActionClick

- (NSError*)execute {
  NSError* error;

  ElementSelector* selector = [self selectorForTarget:&error];
  if (error) {
    return error;
  }

  [self tapOnTarget:selector];
  return nil;
}

@end

@implementation AutomationActionLoadPage

- (NSError*)execute {
  // loadPage is a no-op action - perform nothing
  return nil;
}

@end

@implementation AutomationActionWaitFor

- (NSError*)execute {
  const base::Value* assertionsValue(self.actionDictionary->FindKeyOfType(
      "assertions", base::Value::Type::LIST));
  if (!assertionsValue) {
    return testing::NSErrorWithLocalizedDescription(
        @"Assertions key is missing in WaitFor action.");
  }

  const base::Value::ListStorage& assertionsValues(assertionsValue->GetList());
  if (!assertionsValues.size()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Assertions list is empty in WaitFor action.");
  }

  std::vector<std::string> state_assertions;

  for (auto const& assertionValue : assertionsValues) {
    const std::string assertionString(assertionValue.GetString());
    if (assertionString.empty()) {
      return testing::NSErrorWithLocalizedDescription(
          @"assertionsString is empty in WaitFor action.");
    }
    state_assertions.push_back(assertionString);
  }

  bool success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^{
        return [self CheckForJsAssertionFailures:state_assertions] == nil;
      });
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"waitFor State change hasn't completed within timeout.");
  }

  return nil;
}

// Executes a vector of Javascript assertions on the webpage, returning the
// first assertion that fails to be true, or nil if all assertions are true.
- (NSString*)CheckForJsAssertionFailures:
    (const std::vector<std::string>&)assertions {
  for (std::string const& assertion : assertions) {
    NSError* error;
    NSString* assertionString = base::SysUTF8ToNSString(assertion);

    NSNumber* result =
        base::mac::ObjCCastStrict<NSNumber>(chrome_test_util::ExecuteJavaScript(
            [NSString stringWithFormat:@""
                                        "    (function() {"
                                        "      try {"
                                        "        %@"
                                        "      } catch (ex) {}"
                                        "      return false;"
                                        "    })();",
                                       assertionString],
            &error));

    if (![result boolValue] || error) {
      return assertionString;
    }
  }
  return nil;
}

@end

@implementation AutomationActionAutofill

- (NSError*)execute {
  // The autofill profile is configured in
  // automation_egtest::prepareAutofillProfileWithValues.
  NSError* error = nil;

  ElementSelector* selector = [self selectorForTarget:&error];
  if (error) {
    return error;
  }

  [self tapOnTarget:selector];

  // Tap on the autofill suggestion to perform the actual autofill.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kFormSuggestionLabelAccessibilityIdentifier)]
      performAction:grey_tap()];
  return nil;
}

@end

@implementation AutomationActionValidateField

- (NSError*)execute {
  NSError* error = nil;

  ElementSelector* selector = [self selectorForTarget:&error];
  if (error) {
    return error;
  }

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebStateContainingElement:selector];

  NSString* expectedType = base::SysUTF8ToNSString([self
      getStringFromDictionaryWithKey:"expectedAutofillType"
                               error:&error]);
  if (error) {
    return error;
  }

  NSString* expectedValue = base::SysUTF8ToNSString(
      [self getStringFromDictionaryWithKey:"expectedValue" error:&error]);
  if (error) {
    return error;
  }

  NSString* predictionType = base::mac::ObjCCastStrict<NSString>([self
      executeJavascript:"return target.placeholder;"
               onTarget:selector
                  error:&error]);
  if (error) {
    return error;
  }

  NSString* autofilledValue = base::mac::ObjCCastStrict<NSString>([self
      executeJavascript:"return target.value;"
               onTarget:selector
                  error:&error]);
  if (error) {
    return error;
  }

  if (![predictionType isEqualToString:expectedType]) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Expected prediction type %@ but got %@",
                                   expectedType, predictionType]);
  }
  if (![autofilledValue isEqualToString:expectedValue]) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Expected autofilled value %@ but got %@",
                                   autofilledValue, expectedValue]);
  }
  return nil;
}

@end

@implementation AutomationActionSelectDropdown

- (NSError*)execute {
  NSError* error = nil;

  ElementSelector* selector = [self selectorForTarget:&error];
  if (error) {
    return error;
  }

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebStateContainingElement:selector];

  int selectedIndex = [self getIntFromDictionaryWithKey:"index" error:&error];
  if (error) {
    return error;
  }

  [self executeJavascript:
            base::SysNSStringToUTF8([NSString
                stringWithFormat:@"target.options.selectedIndex = %d; "
                                 @"triggerOnChangeEventOnElement(target);",
                                 selectedIndex])
                 onTarget:selector
                    error:&error];

  return error;
}

@end

@implementation AutomationActionUnrecognized

- (NSError*)execute {
  const base::Value* typeValue =
      self.actionDictionary->FindKeyOfType("type", base::Value::Type::STRING);
  const std::string type(typeValue->GetString());

  return testing::NSErrorWithLocalizedDescription(
      [NSString stringWithFormat:@"Unknown action of type %s", type.c_str()]);
}

@end

@implementation AutomationActionType

- (NSError*)execute {
  NSError* error = nil;

  ElementSelector* selector = [self selectorForTarget:&error];
  if (error) {
    return error;
  }

  std::string value = [self getStringFromDictionaryWithKey:"value"
                                                     error:&error];
  if (error) {
    return error;
  }

  [self executeJavascript:
            base::SysNSStringToUTF8([NSString
                stringWithFormat:
                    @"__gCrWeb.fill.setInputElementValue(\"%s\", target);",
                    value.c_str()])
                 onTarget:selector
                    error:&error];
  return error;
}

@end

@implementation AutomationActionConfirmInfobar

- (NSError*)execute {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kConfirmInfobarButton1AccessibilityIdentifier)]
      performAction:grey_tap()];
  return nil;
}

@end

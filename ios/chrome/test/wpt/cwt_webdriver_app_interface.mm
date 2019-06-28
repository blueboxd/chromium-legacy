// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/testing/nserror_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

NSString* GetIdForWebState(web::WebState* web_state) {
  TabIdTabHelper::CreateForWebState(web_state);
  return TabIdTabHelper::FromWebState(web_state)->tab_id();
}

WebStateList* GetCurrentWebStateList() {
  return chrome_test_util::GetMainController()
      .interfaceProvider.currentInterface.tabModel.webStateList;
}

}  // namespace

@implementation CWTWebDriverAppInterface

+ (NSError*)loadURL:(NSString*)URL timeoutInSeconds:(NSTimeInterval)timeout {
  grey_dispatch_sync_on_main_thread(^{
    chrome_test_util::LoadUrl(GURL(base::SysNSStringToUTF8(URL)));
  });

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL isLoading = NO;
    grey_dispatch_sync_on_main_thread(^{
      isLoading = chrome_test_util::GetCurrentWebState()->IsLoading();
    });
    return !isLoading;
  });

  if (success)
    return nil;

  return testing::NSErrorWithLocalizedDescription(@"Page load timed out");
}

+ (NSString*)getCurrentTabID {
  __block NSString* tabID = nil;
  grey_dispatch_sync_on_main_thread(^{
    web::WebState* webState = chrome_test_util::GetCurrentWebState();
    if (webState)
      tabID = GetIdForWebState(webState);
  });

  return tabID;
}

+ (NSArray*)getTabIDs {
  __block NSMutableArray* tabIDs;
  grey_dispatch_sync_on_main_thread(^{
    DCHECK(!chrome_test_util::IsIncognitoMode());
    WebStateList* webStateList = GetCurrentWebStateList();
    tabIDs = [NSMutableArray arrayWithCapacity:webStateList->count()];

    for (int i = 0; i < webStateList->count(); ++i) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      [tabIDs addObject:GetIdForWebState(webState)];
    }
  });

  return tabIDs;
}

+ (NSError*)closeCurrentTab {
  __block NSError* error = nil;
  grey_dispatch_sync_on_main_thread(^{
    web::WebState* webState = chrome_test_util::GetCurrentWebState();
    if (webState) {
      WebStateList* webStateList = GetCurrentWebStateList();
      webStateList->CloseWebStateAt(webStateList->GetIndexOfWebState(webState),
                                    WebStateList::CLOSE_USER_ACTION);
    } else {
      error = testing::NSErrorWithLocalizedDescription(@"No current tab");
    }
  });

  return error;
}

+ (NSError*)switchToTabWithID:(NSString*)ID {
  __block NSError* error = nil;
  grey_dispatch_sync_on_main_thread(^{
    DCHECK(!chrome_test_util::IsIncognitoMode());
    WebStateList* webStateList = GetCurrentWebStateList();

    for (int i = 0; i < webStateList->count(); ++i) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      if ([ID isEqualToString:GetIdForWebState(webState)]) {
        webStateList->ActivateWebStateAt(i);
        return;
      }
    }
    error = testing::NSErrorWithLocalizedDescription(@"No matching tab");
  });

  return error;
}

+ (NSString*)executeAsyncJavaScriptFunction:(NSString*)function
                           timeoutInSeconds:(NSTimeInterval)timeout {
  const std::string kMessageResultKey("result");

  // Use a distinct messageID value for each invocation of this method to
  // distinguish stale messages (from previous script invocations that timed
  // out) from a message for the current script.
  static NSUInteger messageID = 0;
  std::string command = base::StringPrintf("CWTWebDriver%lu", messageID++);

  // Construct a completion handler that takes a single argument and sends a
  // message with this argument.
  std::string scriptCompletionHandler =
      base::StringPrintf("function(value) {"
                         "__gCrWeb.message.invokeOnHost({command: "
                         "'%s.result', %s: value}); }",
                         command.c_str(), kMessageResultKey.c_str());

  // Construct a script that calls the given |function| with
  // |scriptCompletionHandler| as an argument.
  std::string scriptFunctionWithCompletionHandler = base::StringPrintf(
      "(%s).call(null, %s)", base::SysNSStringToUTF8(function).c_str(),
      scriptCompletionHandler.c_str());

  // Remember the ID of the WebState we inject into since script execution can
  // change the current WebState (e.g., by opening or closing tabs).
  __block NSString* currentWebStateID = nil;

  __block base::Optional<base::Value> messageValue;
  const web::WebState::ScriptCommandCallback callback =
      base::BindRepeating(^bool(const base::DictionaryValue& value, const GURL&,
                                /*interacted*/ bool, /*is_main_frame*/ bool,
                                /*sender_frame*/ web::WebFrame*) {
        const base::Value* result = value.FindKey(kMessageResultKey);

        // |result| will be null when the computed result in JavaScript is
        // |undefined|. This happens, for example, when injecting a script that
        // performs some action (like setting the document's title) but doesn't
        // return any value.
        if (result)
          messageValue = result->Clone();
        else
          messageValue = base::Value();
        return true;
      });

  grey_dispatch_sync_on_main_thread(^{
    web::WebState* webState = chrome_test_util::GetCurrentWebState();
    if (!webState)
      return;
    currentWebStateID = GetIdForWebState(webState);
    webState->AddScriptCommandCallback(callback, command);
    webState->ExecuteJavaScript(
        base::UTF8ToUTF16(scriptFunctionWithCompletionHandler));
  });

  if (!currentWebStateID)
    return nil;

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL scriptExecutionComplete = NO;
    grey_dispatch_sync_on_main_thread(^{
      scriptExecutionComplete = messageValue.has_value();
    });
    return scriptExecutionComplete;
  });

  grey_dispatch_sync_on_main_thread(^{
    WebStateList* webStateList = GetCurrentWebStateList();

    for (int i = 0; i < webStateList->count(); ++i) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      if ([currentWebStateID isEqualToString:GetIdForWebState(webState)]) {
        webState->RemoveScriptCommandCallback(command);
        return;
      }
    }
  });

  if (!success)
    return nil;

  std::string resultAsJSON;
  base::JSONWriter::Write(*messageValue, &resultAsJSON);
  return base::SysUTF8ToNSString(resultAsJSON);
}

+ (void)enablePopups {
  grey_dispatch_sync_on_main_thread(^{
    chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
  });
}

@end

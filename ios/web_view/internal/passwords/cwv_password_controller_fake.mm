// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_password_controller_fake.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/autofill/core/common/renderer_id.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVPasswordControllerFake {
  NSMutableDictionary<NSString*, NSMutableArray<CWVAutofillSuggestion*>*>*
      _suggestionsByKey;
}

- (void)addPasswordSuggestion:(CWVAutofillSuggestion*)suggestion
                     formName:(NSString*)formName
              fieldIdentifier:(NSString*)fieldIdentifier
                      frameID:(NSString*)frameID {
  if (!_suggestionsByKey) {
    _suggestionsByKey = [NSMutableDictionary dictionary];
  }

  NSString* key = [self suggestionsKeyForFormName:formName
                                  fieldIdentifier:fieldIdentifier
                                          frameID:frameID];
  NSMutableArray* suggestions = _suggestionsByKey[key];
  if (!suggestions) {
    suggestions = [NSMutableArray array];
    _suggestionsByKey[key] = suggestions;
  }

  [suggestions addObject:suggestion];
}

- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                           uniqueFormID:(autofill::FormRendererId)uniqueFormID
                        fieldIdentifier:(NSString*)fieldIdentifier
                          uniqueFieldID:(autofill::FieldRendererId)uniqueFieldID
                              fieldType:(NSString*)fieldType
                                frameID:(NSString*)frameID
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>*))
                              completionHandler {
  base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                   NSString* key =
                       [self suggestionsKeyForFormName:formName
                                       fieldIdentifier:fieldIdentifier
                                               frameID:frameID];
                   completionHandler(_suggestionsByKey[key] ?: @[]);
                 }));
}

// Returns a unique key based on the parameters.
- (NSString*)suggestionsKeyForFormName:(NSString*)formName
                       fieldIdentifier:(NSString*)fieldIdentifier
                               frameID:(NSString*)frameID {
  return [NSString
      stringWithFormat:@"%@ %@ %@", formName, fieldIdentifier, frameID];
}

@end

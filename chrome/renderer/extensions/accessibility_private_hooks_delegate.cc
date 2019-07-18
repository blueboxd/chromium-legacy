// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/accessibility_private_hooks_delegate.h"

#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace extensions {

namespace {
constexpr char kGetDisplayLanguage[] =
    "accessibilityPrivate.getDisplayLanguage";
constexpr char kUndeterminedLanguage[] = "und";
}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

AccessibilityPrivateHooksDelegate::AccessibilityPrivateHooksDelegate() =
    default;
AccessibilityPrivateHooksDelegate::~AccessibilityPrivateHooksDelegate() =
    default;

RequestResult AccessibilityPrivateHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  // Error checks.
  // Ensure we would like to call the GetDisplayLanguage function.
  if (method_name != kGetDisplayLanguage)
    return RequestResult(RequestResult::NOT_HANDLED);
  // Ensure arguments are successfully parsed and converted.
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }
  return HandleGetDisplayLanguage(GetScriptContextFromV8ContextChecked(context),
                                  *parse_result.arguments);
}

// Called to translate a language code into human-readable string in the
// language of the provided language code. Language codes can have optional
// country code e.g. 'en' and 'en-us' both yield an output of 'English'.
// As another example, the code 'fr' would produce 'francais' as output.
RequestResult AccessibilityPrivateHooksDelegate::HandleGetDisplayLanguage(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& parsed_arguments) {
  DCHECK(script_context->extension());
  DCHECK_EQ(1u, parsed_arguments.size());
  DCHECK(parsed_arguments[0]->IsString());
  icu::Locale locale = icu::Locale(
      gin::V8ToString(script_context->isolate(), parsed_arguments[0]).c_str());
  icu::UnicodeString display_language;
  locale.getDisplayLanguage(locale, display_language);
  std::string language_result =
      base::UTF16ToUTF8(base::i18n::UnicodeStringToString16(display_language));
  // Instead of returning "und", which is what the ICU Locale class returns for
  // undetermined languages, we would simply like to return an empty string
  // to communicate that we could not determine the display language.
  if (language_result == kUndeterminedLanguage)
    language_result = "";
  RequestResult result(RequestResult::HANDLED);
  result.return_value =
      gin::StringToV8(script_context->isolate(), language_result);
  return result;
}

}  // namespace extensions

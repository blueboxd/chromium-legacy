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
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {
constexpr char kGetDisplayNameForLocale[] =
    "accessibilityPrivate.getDisplayNameForLocale";
constexpr char kUndeterminedLocale[] = "und";
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
  // Ensure we would like to call the GetDisplayNameForLocale function.
  if (method_name != kGetDisplayNameForLocale)
    return RequestResult(RequestResult::NOT_HANDLED);
  // Ensure arguments are successfully parsed and converted.
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }
  return HandleGetDisplayNameForLocale(
      GetScriptContextFromV8ContextChecked(context), *parse_result.arguments);
}

RequestResult AccessibilityPrivateHooksDelegate::HandleGetDisplayNameForLocale(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& parsed_arguments) {
  DCHECK(script_context->extension());
  DCHECK_EQ(2u, parsed_arguments.size());
  DCHECK(parsed_arguments[0]->IsString());
  DCHECK(parsed_arguments[1]->IsString());

  const std::string locale =
      gin::V8ToString(script_context->isolate(), parsed_arguments[0]);
  const std::string display_locale =
      gin::V8ToString(script_context->isolate(), parsed_arguments[1]);

  std::string locale_result =
      base::UTF16ToUTF8(l10n_util::GetDisplayNameForLocale(
          locale, display_locale, true /* is_ui */,
          true /* disallow_default */));

  RequestResult result(RequestResult::HANDLED);

  // Instead of returning "und", which is what the ICU Locale class returns for
  // undetermined locales, we would simply like to return an empty string to
  // communicate that we could not determine the display locale. In addition,
  // ICU returns the |locale| as |result_locale| if it has no translation for a
  // valid locale. Also return an empty string for that case.
  if (locale_result == kUndeterminedLocale || locale_result == locale)
    locale_result = std::string();

  result.return_value =
      gin::StringToV8(script_context->isolate(), locale_result);
  return result;
}

}  // namespace extensions

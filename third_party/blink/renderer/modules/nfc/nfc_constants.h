// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_CONSTANTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_CONSTANTS_H_

#include "third_party/blink/renderer/bindings/modules/v8/string_or_array_buffer_or_ndef_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_unrestricted_double_or_array_buffer_or_dictionary.h"

namespace blink {

using NDEFMessageSource = blink::StringOrArrayBufferOrNDEFMessage;
using NDEFRecordData =
    blink::StringOrUnrestrictedDoubleOrArrayBufferOrDictionary;

extern const char kNfcJsonMimePostfix[];
extern const char kNfcJsonMimePrefix[];
extern const char kNfcJsonMimeType[];
extern const char kNfcOpaqueMimeType[];
extern const char kNfcPlainTextMimeType[];
extern const char kNfcPlainTextMimePrefix[];
extern const char kNfcProtocolHttps[];
extern const char kNfcCharSetUTF8[];

// Error messages.
extern const char kNfcNotSupported[];
extern const char kNfcTextRecordTypeError[];
extern const char kNfcSetIdError[];
extern const char kNfcTextRecordMediaTypeError[];
extern const char kNfcUrlRecordTypeError[];
extern const char kNfcUrlRecordParseError[];
extern const char kNfcJsonRecordTypeError[];
extern const char kNfcJsonRecordMediaTypeError[];
extern const char kNfcOpaqueRecordTypeError[];
extern const char kNfcRecordTypeError[];
extern const char kNfcRecordDataError[];
extern const char kNfcRecordError[];
extern const char kNfcMsgTypeError[];
extern const char kNfcEmptyMsg[];
extern const char kNfcMsgConvertError[];
extern const char kNfcMsgMaxSizeError[];
extern const char kNfcUrlPatternError[];
extern const char kNfcInvalidPushTimeout[];
extern const char kNfcWatchIdNotFound[];

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_CONSTANTS_H_

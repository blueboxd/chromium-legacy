// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_CONSTANTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_CONSTANTS_H_


namespace blink {

extern const char kNfcJsonMimePostfix[];
extern const char kNfcJsonMimePrefix[];
extern const char kNfcJsonMimeType[];
extern const char kNfcJsonTextMimeType[];
extern const char kNfcOpaqueMimeType[];
extern const char kNfcPlainTextMimeType[];
extern const char kNfcPlainTextMimePrefix[];
extern const char kNfcProtocolHttps[];
extern const char kNfcCharSetUTF8[];

// Error messages.
extern const char kNfcNotAllowed[];
extern const char kNfcNotSupported[];
extern const char kNfcNotReadable[];
extern const char kNfcTextRecordTypeError[];
extern const char kNfcSetIdError[];
extern const char kNfcTextRecordMediaTypeError[];
extern const char kNfcUrlRecordTypeError[];
extern const char kNfcUrlRecordParseError[];
extern const char kNfcJsonRecordNoDataError[];
extern const char kNfcJsonRecordStringifyError[];
extern const char kNfcJsonRecordMediaTypeError[];
extern const char kNfcOpaqueRecordTypeError[];
extern const char kNfcRecordTypeError[];
extern const char kNfcRecordDataError[];
extern const char kNfcRecordError[];
extern const char kNfcMsgTypeError[];
extern const char kNfcEmptyMsg[];
extern const char kNfcInvalidMsg[];
extern const char kNfcMsgConvertError[];
extern const char kNfcMsgMaxSizeError[];
extern const char kNfcUrlPatternError[];
extern const char kNfcInvalidPushTimeout[];
extern const char kNfcWatchIdNotFound[];
extern const char kNfcAccessInNonTopFrame[];
extern const char kNfcCancelled[];
extern const char kNfcTimeout[];
extern const char kNfcUnknownError[];
extern const char kNfcDataTransferError[];
extern const char kNfcNoModificationAllowed[];

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_CONSTANTS_H_

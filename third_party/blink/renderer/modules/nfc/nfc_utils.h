// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_UTILS_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

ScriptPromise RejectIfInvalidTextRecord(ScriptState* script_state,
                                        const NDEFRecord* record);

ScriptPromise RejectIfInvalidURLRecord(ScriptState* script_state,
                                       const NDEFRecord* record);

ScriptPromise RejectIfInvalidJSONRecord(ScriptState* script_state,
                                        const NDEFRecord* record);

ScriptPromise RejectIfInvalidOpaqueRecord(ScriptState* script_state,
                                          const NDEFRecord* record);

ScriptPromise RejectIfInvalidNDEFRecord(ScriptState* script_state,
                                        const NDEFRecord* record);

ScriptPromise RejectIfInvalidNDEFRecordArray(
    ScriptState* script_state,
    const HeapVector<Member<NDEFRecord>>& records);

ScriptPromise RejectIfInvalidNDEFMessageSource(
    ScriptState* script_state,
    const NDEFMessageSource& push_message);

NDEFRecordData BuildRecordData(
    ScriptState* script_state,
    const device::mojom::blink::NDEFRecordPtr& record);

device::mojom::blink::NDEFRecordType DeduceRecordTypeFromDataType(
    const blink::NDEFRecord* record);

size_t GetNDEFMessageSize(const device::mojom::blink::NDEFMessagePtr& message);

bool SetNDEFMessageURL(const String& origin,
                       device::mojom::blink::NDEFMessagePtr& message);

WTF::String NDEFRecordTypeToString(
    const device::mojom::blink::NDEFRecordType& type);

device::mojom::blink::NDEFCompatibility StringToNDEFCompatibility(
    const WTF::String& compatibility);

device::mojom::blink::NDEFRecordType StringToNDEFRecordType(
    const WTF::String& recordType);

device::mojom::blink::NFCPushTarget StringToNFCPushTarget(
    const WTF::String& target);

NDEFRecord* MojoToBlinkNDEFRecord(
    ScriptState* script_state,
    const device::mojom::blink::NDEFRecordPtr& record);

NDEFMessage* MojoToBlinkNDEFMessage(
    ScriptState* script_state,
    const device::mojom::blink::NDEFMessagePtr& message);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_UTILS_H_

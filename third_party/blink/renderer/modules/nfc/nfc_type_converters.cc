// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"
#include "third_party/blink/renderer/modules/nfc/nfc_push_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_scan_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using device::mojom::blink::NDEFMessage;
using device::mojom::blink::NDEFMessagePtr;
using device::mojom::blink::NDEFRecord;
using device::mojom::blink::NDEFRecordPtr;
using device::mojom::blink::NDEFRecordTypeFilter;
using device::mojom::blink::NFCPushOptions;
using device::mojom::blink::NFCPushOptionsPtr;
using device::mojom::blink::NFCScanOptions;
using device::mojom::blink::NFCScanOptionsPtr;

// Mojo type converters
namespace mojo {

NDEFRecordPtr TypeConverter<NDEFRecordPtr, ::blink::NDEFRecord*>::Convert(
    const ::blink::NDEFRecord* record) {
  return NDEFRecord::New(record->recordType(), record->mediaType(),
                         record->data());
}

NDEFMessagePtr TypeConverter<NDEFMessagePtr, ::blink::NDEFMessage*>::Convert(
    const ::blink::NDEFMessage* message) {
  NDEFMessagePtr messagePtr = NDEFMessage::New();
  messagePtr->url = message->url();
  messagePtr->data.resize(message->records().size());
  for (wtf_size_t i = 0; i < message->records().size(); ++i) {
    NDEFRecordPtr record = NDEFRecord::From(message->records()[i].Get());
    DCHECK(record);
    messagePtr->data[i] = std::move(record);
  }
  return messagePtr;
}

NFCPushOptionsPtr
TypeConverter<NFCPushOptionsPtr, const blink::NFCPushOptions*>::Convert(
    const blink::NFCPushOptions* pushOptions) {
  // https://w3c.github.io/web-nfc/#the-nfcpushoptions-dictionary
  // Default values for NFCPushOptions dictionary are:
  // target = 'any', timeout = Infinity, ignoreRead = true
  NFCPushOptionsPtr pushOptionsPtr = NFCPushOptions::New();
  pushOptionsPtr->target = blink::StringToNFCPushTarget(pushOptions->target());
  pushOptionsPtr->ignore_read = pushOptions->ignoreRead();

  if (pushOptions->hasTimeout())
    pushOptionsPtr->timeout = pushOptions->timeout();
  else
    pushOptionsPtr->timeout = std::numeric_limits<double>::infinity();

  return pushOptionsPtr;
}

NFCScanOptionsPtr
TypeConverter<NFCScanOptionsPtr, const blink::NFCScanOptions*>::Convert(
    const blink::NFCScanOptions* scanOptions) {
  // https://w3c.github.io/web-nfc/#dom-nfcscanoptions
  // Default values for NFCScanOptions dictionary are:
  // url = "", recordType = null, mediaType = ""
  NFCScanOptionsPtr scanOptionsPtr = NFCScanOptions::New();
  scanOptionsPtr->url = scanOptions->url();
  scanOptionsPtr->media_type = scanOptions->mediaType();

  if (scanOptions->hasRecordType()) {
    scanOptionsPtr->record_filter = NDEFRecordTypeFilter::New();
    scanOptionsPtr->record_filter->record_type = scanOptions->recordType();
  }

  return scanOptionsPtr;
}

}  // namespace mojo

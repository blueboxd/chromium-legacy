// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PROXY_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class Document;
class NFCReader;

// This is a proxy class used by NFCWriter(s) and NFCReader(s) to connect
// to the DeviceService for device::mojom::blink::NFC service.
class MODULES_EXPORT NFCProxy final
    : public GarbageCollectedFinalized<NFCProxy>,
      public PageVisibilityObserver,
      public Supplement<Document>,
      public device::mojom::blink::NFCClient {
  USING_GARBAGE_COLLECTED_MIXIN(NFCProxy);
  USING_PRE_FINALIZER(NFCProxy, Dispose);

 public:
  static const char kSupplementName[];
  static NFCProxy* From(Document&);

  explicit NFCProxy(Document&);
  ~NFCProxy() override;

  void Dispose();

  void Trace(blink::Visitor*) override;

  void AddReader(NFCReader*);
  void RemoveReader(NFCReader*);
  void Push(device::mojom::blink::NDEFMessagePtr message,
            device::mojom::blink::NFCPushOptionsPtr options,
            device::mojom::blink::NFC::PushCallback cb);

  // Implementation of PageVisibilityObserver.
  void PageVisibilityChanged() override;

 private:
  // Implementation of device::mojom::blink::NFCClient.
  void OnWatch(const Vector<uint32_t>& ids,
               device::mojom::blink::NDEFMessagePtr) override;

  void OnReaderRegistered(NFCReader* reader,
                          uint32_t id,
                          device::mojom::blink::NFCErrorPtr error);
  void OnCancelWatch(NFCReader* reader,
                     device::mojom::blink::NFCErrorPtr error);

  // For the Mojo connection with the DeviceService.
  void EnsureMojoConnection();
  void OnMojoConnectionError();

  // The <NFCReader, WatchId> map.
  using ReaderMap = HeapHashMap<WeakMember<NFCReader>, uint32_t>;
  ReaderMap readers_;

  device::mojom::blink::NFCPtr nfc_;
  mojo::Binding<device::mojom::blink::NFCClient> client_binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PROXY_H_

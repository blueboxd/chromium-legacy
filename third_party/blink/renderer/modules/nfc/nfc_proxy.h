// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PROXY_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class Document;
class NFCReader;
class NFCWriter;

// This is a proxy class used by NFCWriter(s) and NFCReader(s) to connect
// to the DeviceService for device::mojom::blink::NFC service.
class MODULES_EXPORT NFCProxy final
    : public GarbageCollectedFinalized<NFCProxy>,
      public PageVisibilityObserver,
      public FocusChangedObserver,
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

  // There is no matching RemoveWriter() method because writers are
  // automatically removed from the weak hash set when they are garbage
  // collected.
  void AddWriter(NFCWriter*);

  void StartReading(NFCReader*);
  void StopReading(NFCReader*);
  bool IsReading(const NFCReader*);
  void Push(device::mojom::blink::NDEFMessagePtr message,
            device::mojom::blink::NFCPushOptionsPtr options,
            device::mojom::blink::NFC::PushCallback cb);
  void CancelPush(const String& target,
                  device::mojom::blink::NFC::CancelPushCallback callback);

 private:
  // Implementation of device::mojom::blink::NFCClient.
  void OnWatch(const Vector<uint32_t>&,
               const String&,
               device::mojom::blink::NDEFMessagePtr) override;

  void OnReaderRegistered(NFCReader* reader,
                          uint32_t id,
                          device::mojom::blink::NFCErrorPtr error);

  // Implementation of PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // Implementation of FocusChangedObserver.
  void FocusedFrameChanged() override;

  void UpdateSuspendedStatus();
  bool ShouldSuspendNFC() const;

  // For the Mojo connection with the DeviceService.
  void EnsureMojoConnection();
  void OnMojoConnectionError();

  // The <NFCReader, WatchId> map. An reader is inserted with the WatchId
  // initially being 0, then we send a watch request to |nfc_|, the watch ID is
  // then updated to the value (>= 1) passed to OnReaderRegistered().
  using ReaderMap = HeapHashMap<WeakMember<NFCReader>, uint32_t>;
  ReaderMap readers_;

  using WriterSet = HeapHashSet<WeakMember<NFCWriter>>;
  WriterSet writers_;

  device::mojom::blink::NFCPtr nfc_;
  mojo::Binding<device::mojom::blink::NFCClient> client_binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PROXY_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_message_callback.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/nfc/nfc_constants.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class NFCPushOptions;
class NFCReaderOptions;
class ScriptPromiseResolver;

class NFC final : public ScriptWrappable,
                  public PageVisibilityObserver,
                  public ContextLifecycleObserver,
                  public device::mojom::blink::NFCClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(NFC);
  USING_PRE_FINALIZER(NFC, Dispose);

 public:
  static NFC* Create(LocalFrame*);

  explicit NFC(LocalFrame*);
  ~NFC() override;

  void Dispose();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  // Pushes NDEFMessageSource asynchronously to NFC tag / peer.
  ScriptPromise push(ScriptState*,
                     const NDEFMessageSource&,
                     const NFCPushOptions*);

  // Cancels ongoing push operation.
  ScriptPromise cancelPush(ScriptState*, const String&);

  // Starts watching for NFC messages that match NFCReaderOptions criteria.
  ScriptPromise watch(ScriptState*,
                      V8MessageCallback*,
                      const NFCReaderOptions*);

  // Cancels watch operation with id.
  ScriptPromise cancelWatch(ScriptState*, int32_t id);

  // Cancels all watch operations.
  ScriptPromise cancelWatch(ScriptState*);

  // Implementation of PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // Interface required by garbage collection.
  void Trace(blink::Visitor*) override;

 private:
  // Returns boolean indicating whether NFC is supported in this context. If
  // not, |error_message| gives a message indicating why not.
  bool IsSupportedInContext(ExecutionContext*, String& error_message);

  // Returns promise with DOMException if feature is not supported
  // or when context is not secure. Otherwise, returns empty promise.
  ScriptPromise RejectIfNotSupported(ScriptState*);

  void OnRequestCompleted(ScriptPromiseResolver*,
                          device::mojom::blink::NFCErrorPtr);
  void OnConnectionError();
  void OnWatchRegistered(V8MessageCallback*,
                         ScriptPromiseResolver*,
                         uint32_t id,
                         device::mojom::blink::NFCErrorPtr);

  // device::mojom::blink::NFCClient implementation.
  void OnWatch(const Vector<uint32_t>&,
               const String&,
               device::mojom::blink::NDEFMessagePtr) override;

 private:
  device::mojom::blink::NFCPtr nfc_;
  mojo::Binding<device::mojom::blink::NFCClient> client_binding_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  using WatchCallbacksMap = HeapHashMap<uint32_t, Member<V8MessageCallback>>;
  WatchCallbacksMap callbacks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_READER_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class NFCProxy;
class NFCReaderOptions;

class MODULES_EXPORT NFCReader : public EventTargetWithInlineData,
                                 public ActiveScriptWrappable<NFCReader>,
                                 public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NFCReader);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NFCReader* Create(ExecutionContext*, NFCReaderOptions*);

  NFCReader(ExecutionContext*, NFCReaderOptions*);
  ~NFCReader() override;

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable overrides.
  bool HasPendingActivity() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(reading, kReading)
  void start();
  void stop();

  void Trace(blink::Visitor*) override;

  const NFCReaderOptions* options() const { return options_; }

  // Called by NFCProxy for dispatching events.
  virtual void OnReading(const String& serial_number,
                         const device::mojom::blink::NDEFMessage& message);
  virtual void OnError(device::mojom::blink::NFCErrorType error);

 private:
  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  NFCProxy* GetNfcProxy() const;

  bool CheckSecurity();

  const Member<NFCReaderOptions> options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_READER_H_

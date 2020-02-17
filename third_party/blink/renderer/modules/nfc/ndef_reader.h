// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class NFCProxy;
class NDEFScanOptions;
class ScriptPromiseResolver;

class MODULES_EXPORT NDEFReader : public EventTargetWithInlineData,
                                  public ActiveScriptWrappable<NDEFReader>,
                                  public ExecutionContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NDEFReader);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NDEFReader* Create(ExecutionContext*);

  NDEFReader(ExecutionContext*);
  ~NDEFReader() override;

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable overrides.
  bool HasPendingActivity() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(reading, kReading)
  ScriptPromise scan(ScriptState*, const NDEFScanOptions*, ExceptionState&);

  void Trace(Visitor*) override;

  // Called by NFCProxy for dispatching events.
  virtual void OnReading(const String& serial_number,
                         const device::mojom::blink::NDEFMessage&);
  virtual void OnError(const String& message);

  // Called by NFCProxy for notification about connection error.
  void OnMojoConnectionError();

 private:
  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  void Abort(ScriptPromiseResolver*);

  NFCProxy* GetNfcProxy() const;

  // Permission handling
  void OnRequestPermission(ScriptPromiseResolver* resolver,
                           const NDEFScanOptions* options,
                           mojom::blink::PermissionStatus status);
  mojom::blink::PermissionService* GetPermissionService();
  mojo::Remote<mojom::blink::PermissionService> permission_service_;

  // |resolver_| is kept here to handle Mojo connection failures because in that
  // case the callback passed to Watch() won't be called and
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/serial.mojom-blink.h"
#include "third_party/blink/public/mojom/serial/serial.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace base {
class UnguessableToken;
}

namespace blink {

class ReadableStream;
class ScriptPromiseResolver;
class ScriptState;
class Serial;
class SerialOptions;
class SerialPortUnderlyingSink;
class SerialPortUnderlyingSource;
class WritableStream;

class SerialPort final : public ScriptWrappable,
                         public device::mojom::blink::SerialPortClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(SerialPort, Dispose);

 public:
  explicit SerialPort(Serial* parent, mojom::blink::SerialPortInfoPtr info);
  ~SerialPort() override;

  // Web-exposed functions
  ReadableStream* readable() const { return readable_; }
  WritableStream* writable() const { return writable_; }

  ScriptPromise open(ScriptState*, const SerialOptions* options);
  void clearReadError(ScriptState*, ExceptionState&);
  void close();

  const base::UnguessableToken& token() const { return info_->token; }

  void UnderlyingSourceClosed();
  void UnderlyingSinkClosed();

  void ContextDestroyed();
  void Trace(Visitor*) override;
  void Dispose();

  // SerialPortClient
  void OnReadError(device::mojom::blink::SerialReceiveError) override;
  void OnSendError(device::mojom::blink::SerialSendError) override;

 private:
  bool CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                      mojo::ScopedDataPipeConsumerHandle* consumer);
  void OnConnectionError();
  void OnOpen(mojo::ScopedDataPipeConsumerHandle,
              mojo::ScopedDataPipeProducerHandle,
              device::mojom::blink::SerialPortClientRequest,
              bool success);
  void InitializeReadableStream(ScriptState*,
                                mojo::ScopedDataPipeConsumerHandle);
  void InitializeWritableStream(ScriptState*,
                                mojo::ScopedDataPipeProducerHandle);

  mojom::blink::SerialPortInfoPtr info_;
  Member<Serial> parent_;

  uint32_t buffer_size_ = 0;
  device::mojom::blink::SerialPortPtr port_;
  mojo::Binding<device::mojom::blink::SerialPortClient> client_binding_{this};

  Member<ReadableStream> readable_;
  Member<SerialPortUnderlyingSource> underlying_source_;
  Member<WritableStream> writable_;
  Member<SerialPortUnderlyingSink> underlying_sink_;

  // Resolver for the Promise returned by open().
  Member<ScriptPromiseResolver> open_resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_

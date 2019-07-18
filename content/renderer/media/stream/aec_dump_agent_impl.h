// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_AEC_DUMP_AGENT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_STREAM_AEC_DUMP_AGENT_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/mediastream/aec_dump.mojom.h"

namespace content {

// An instance of this class connects to the browser process to register for
// notifications to start / stop writing to a dump file.
class AecDumpAgentImpl : public blink::mojom::AecDumpAgent {
 public:
  class Delegate {
   public:
    virtual void OnStartDump(base::File file) = 0;
    virtual void OnStopDump() = 0;
  };

  // This may fail in unit tests, in which case a null object is returned.
  static std::unique_ptr<AecDumpAgentImpl> Create(Delegate* delegate);

  ~AecDumpAgentImpl() override;

  // AecDumpAgent methods:
  void Start(base::File dump_file) override;
  void Stop() override;

 private:
  explicit AecDumpAgentImpl(
      Delegate* delegate,
      mojo::PendingReceiver<blink::mojom::AecDumpAgent> receiver);

  Delegate* delegate_;
  mojo::Receiver<blink::mojom::AecDumpAgent> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(AecDumpAgentImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_AEC_DUMP_AGENT_IMPL_H_

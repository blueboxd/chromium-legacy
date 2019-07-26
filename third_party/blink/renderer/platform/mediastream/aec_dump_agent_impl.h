// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_AEC_DUMP_AGENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_AEC_DUMP_AGENT_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/mediastream/aec_dump.mojom-blink.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class AecDumpAgentImplDelegate;

// An instance of this class connects to the browser process to register for
// notifications to start / stop writing to a dump file.
class AecDumpAgentImpl : public mojom::blink::AecDumpAgent {
 public:
  // This may fail in unit tests, in which case a null object is returned.
  static std::unique_ptr<AecDumpAgentImpl> Create(
      AecDumpAgentImplDelegate* delegate);

  ~AecDumpAgentImpl() override;

  // AecDumpAgent methods:
  void Start(base::File dump_file) override;
  void Stop() override;

 private:
  explicit AecDumpAgentImpl(
      AecDumpAgentImplDelegate* delegate,
      mojo::PendingReceiver<mojom::blink::AecDumpAgent> receiver);

  AecDumpAgentImplDelegate* delegate_;
  mojo::Receiver<mojom::blink::AecDumpAgent> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(AecDumpAgentImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_AEC_DUMP_AGENT_IMPL_H_

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_ACCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_ACCESSOR_H_

#include <memory>
#include "base/time/time.h"
#include "media/midi/midi_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/webmidi/midi_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class MIDIAccessorClient;

// TODO(https://crbug.com/582328): Remove this class, and call MIDIDispatcher
// directly.
class MIDIAccessor final {
  USING_FAST_MALLOC(MIDIAccessor);

 public:
  MIDIAccessor(MIDIAccessorClient* client,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MIDIAccessor();

  void StartSession();
  void SendMIDIData(unsigned port_index,
                    const unsigned char* data,
                    wtf_size_t length,
                    base::TimeTicks time_stamp);
  // MIDIAccessInitializer and MIDIAccess are both MIDIAccessClient.
  // MIDIAccessInitializer is the first client and MIDIAccess takes over it
  // once the initialization successfully finishes.
  void SetClient(MIDIAccessorClient* client) { client_ = client; }

  // The following methods are used by MIDIDispatcher to forward messages from
  // the browser process.
  void DidAddInputPort(const String& id,
                       const String& manufacturer,
                       const String& name,
                       const String& version,
                       midi::mojom::PortState);
  void DidAddOutputPort(const String& id,
                        const String& manufacturer,
                        const String& name,
                        const String& version,
                        midi::mojom::PortState);
  void DidSetInputPortState(unsigned port_index, midi::mojom::PortState);
  void DidSetOutputPortState(unsigned port_index, midi::mojom::PortState);
  void DidStartSession(midi::mojom::Result);
  void DidReceiveMIDIData(unsigned port_index,
                          const unsigned char* data,
                          wtf_size_t length,
                          base::TimeTicks time_stamp);

 private:

  MIDIAccessorClient* client_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<MIDIDispatcher> dispatcher_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_ACCESSOR_H_

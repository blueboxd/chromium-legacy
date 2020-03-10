// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header defines utilities for converting between Mojo interface variant
// types. This is useful for maintaining type safety when message pipes need to
// be passed across the Blink public API boundary.
//
// Example conversion from the Blink variant into a cross-variant handle:
//
// namespace blink {
//
// void WebLocalFrameImpl::PassGoatTeleporter() {
//   mojo::PendingRemote<mojom::blink::GoatTeleporter> remote =
//       ProcureGoatTeleporter();
//
//   // CrossVariantMojoReceiver and CrossVariantMojoRemote may created from
//   // any interface variant. Note the use of the unrelated *InterfaceBase
//   // class as the cross-variant handle's template parameter. This is an empty
//   // helper class defined by the .mojom-shared.h header that is common to all
//   // variants of a Mojo interface and is useful for implementing type safety
//   // checks such as this one.
//   web_local_frame_client->PassGoatTeleporter(
//       ToCrossVariantMojoRemote(std::move(cross_variant_remote)));
// }
//
// }  // namespace blink
//
// Example conversion from a cross-variant handle into the regular variant:
//
// namespace content {
//
// void RenderFrameImpl::PassGoatTeleporter(
//     blink::CrossVariantMojoRemote<GoatTeleporterInterfaceBase>
//     cross_variant_remote) {
//   mojo::PendingRemote<blink::mojom::GoatTeleporter> remote =
//       cross_variant_remote
//           .PassAsPendingRemote<blink::mojom::GoatTeleporter>();
// }
//
// }  // namespace content

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CROSS_VARIANT_MOJO_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CROSS_VARIANT_MOJO_UTIL_H_

#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace blink {

template <typename Interface>
class CrossVariantMojoReceiver {
 public:
  CrossVariantMojoReceiver() = default;
  ~CrossVariantMojoReceiver() = default;

  CrossVariantMojoReceiver(CrossVariantMojoReceiver&&) noexcept = default;
  CrossVariantMojoReceiver& operator=(CrossVariantMojoReceiver&&) noexcept =
      default;

  CrossVariantMojoReceiver(const CrossVariantMojoReceiver&) = delete;
  CrossVariantMojoReceiver& operator=(const CrossVariantMojoReceiver&) =
      default;

  template <typename VariantInterface,
            typename CrossVariantBase = typename VariantInterface::Base_,
            std::enable_if_t<
                std::is_same<CrossVariantBase, Interface>::value>* = nullptr>
  CrossVariantMojoReceiver(mojo::PendingReceiver<VariantInterface> receiver)
      : pipe_(receiver.PassPipe()) {}

 private:
  friend struct mojo::PendingReceiverConverter<CrossVariantMojoReceiver>;

  // Constructs a valid CrossVariantMojoReceiver from a valid raw message pipe
  // handle.
  explicit CrossVariantMojoReceiver(mojo::ScopedMessagePipeHandle pipe)
      : pipe_(std::move(pipe)) {
    DCHECK(pipe_.is_valid());
  }

  mojo::ScopedMessagePipeHandle pipe_;
};

template <typename Interface>
class CrossVariantMojoRemote {
 public:
  CrossVariantMojoRemote() = default;
  ~CrossVariantMojoRemote() = default;

  CrossVariantMojoRemote(CrossVariantMojoRemote&&) noexcept = default;
  CrossVariantMojoRemote& operator=(CrossVariantMojoRemote&&) noexcept =
      default;

  CrossVariantMojoRemote(const CrossVariantMojoRemote&) = delete;
  CrossVariantMojoRemote& operator=(const CrossVariantMojoRemote&) = default;

  template <typename VariantInterface,
            typename CrossVariantBase = typename VariantInterface::Base_,
            std::enable_if_t<
                std::is_same<CrossVariantBase, Interface>::value>* = nullptr>
  CrossVariantMojoRemote(mojo::PendingRemote<VariantInterface> remote)
      : version_(remote.version()), pipe_(remote.PassPipe()) {}

 private:
  friend struct mojo::PendingRemoteConverter<CrossVariantMojoRemote>;

  // Constructs a valid CrossVariantMojoRemote from a valid raw message pipe
  // handle.
  explicit CrossVariantMojoRemote(mojo::ScopedMessagePipeHandle pipe,
                                  uint32_t version)
      : pipe_(std::move(pipe)), version_(version) {
    DCHECK(pipe_.is_valid());
  }

  // Subtle: |version_| is ordered before |pipe_| so it can be initialized first
  // in the move conversion constructor. |PendingRemote::PassPipe()| invalidates
  // all other state on PendingRemote so it must be called last.
  uint32_t version_;
  mojo::ScopedMessagePipeHandle pipe_;
};

}  // namespace blink

namespace mojo {

template <typename CrossVariantBase>
struct PendingReceiverConverter<
    blink::CrossVariantMojoReceiver<CrossVariantBase>> {
  template <typename VariantBase>
  static PendingReceiver<VariantBase> To(
      blink::CrossVariantMojoReceiver<CrossVariantBase>&& in) {
    return in.pipe_.is_valid()
               ? PendingReceiver<VariantBase>(std::move(in.pipe_))
               : PendingReceiver<VariantBase>();
  }
};

template <typename CrossVariantBase>
struct PendingRemoteConverter<blink::CrossVariantMojoRemote<CrossVariantBase>> {
  template <typename VariantBase>
  static PendingRemote<VariantBase> To(
      blink::CrossVariantMojoRemote<CrossVariantBase>&& in) {
    return in.pipe_.is_valid()
               ? PendingRemote<VariantBase>(std::move(in.pipe_), in.version_)
               : PendingRemote<VariantBase>();
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CROSS_VARIANT_MOJO_UTIL_H_

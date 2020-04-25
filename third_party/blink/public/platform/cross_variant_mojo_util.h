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
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace blink {

// Non-associated helpers

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

  CrossVariantMojoReceiver(const mojo::NullReceiver&) {}

 private:
  friend struct mojo::PendingReceiverConverter<CrossVariantMojoReceiver>;

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

  CrossVariantMojoRemote(const mojo::NullRemote&) {}

 private:
  friend struct mojo::PendingRemoteConverter<CrossVariantMojoRemote>;

  // Subtle: |version_| is ordered before |pipe_| so it can be initialized first
  // in the move conversion constructor. |PendingRemote::PassPipe()| invalidates
  // all other state on PendingRemote so it must be called last.
  uint32_t version_;
  mojo::ScopedMessagePipeHandle pipe_;
};

// Associated helpers

template <typename Interface>
class CrossVariantMojoAssociatedReceiver {
 public:
  CrossVariantMojoAssociatedReceiver() = default;
  ~CrossVariantMojoAssociatedReceiver() = default;

  CrossVariantMojoAssociatedReceiver(
      CrossVariantMojoAssociatedReceiver&&) noexcept = default;
  CrossVariantMojoAssociatedReceiver& operator=(
      CrossVariantMojoAssociatedReceiver&&) noexcept = default;

  CrossVariantMojoAssociatedReceiver(
      const CrossVariantMojoAssociatedReceiver&) = delete;
  CrossVariantMojoAssociatedReceiver& operator=(
      const CrossVariantMojoAssociatedReceiver&) = default;

  template <typename VariantInterface,
            typename CrossVariantBase = typename VariantInterface::Base_,
            std::enable_if_t<
                std::is_same<CrossVariantBase, Interface>::value>* = nullptr>
  CrossVariantMojoAssociatedReceiver(
      mojo::PendingAssociatedReceiver<VariantInterface> receiver)
      : handle_(receiver.PassHandle()) {}

  CrossVariantMojoAssociatedReceiver(const mojo::NullAssociatedReceiver&) {}

 private:
  friend struct mojo::PendingAssociatedReceiverConverter<
      CrossVariantMojoAssociatedReceiver>;

  mojo::ScopedInterfaceEndpointHandle handle_;
};

template <typename Interface>
class CrossVariantMojoAssociatedRemote {
 public:
  CrossVariantMojoAssociatedRemote() = default;
  ~CrossVariantMojoAssociatedRemote() = default;

  CrossVariantMojoAssociatedRemote(
      CrossVariantMojoAssociatedRemote&&) noexcept = default;
  CrossVariantMojoAssociatedRemote& operator=(
      CrossVariantMojoAssociatedRemote&&) noexcept = default;

  CrossVariantMojoAssociatedRemote(const CrossVariantMojoAssociatedRemote&) =
      delete;
  CrossVariantMojoAssociatedRemote& operator=(
      const CrossVariantMojoAssociatedRemote&) = default;

  template <typename VariantInterface,
            typename CrossVariantBase = typename VariantInterface::Base_,
            std::enable_if_t<
                std::is_same<CrossVariantBase, Interface>::value>* = nullptr>
  CrossVariantMojoAssociatedRemote(
      mojo::PendingAssociatedRemote<VariantInterface> remote)
      : version_(remote.version()), handle_(remote.PassHandle()) {}

  CrossVariantMojoAssociatedRemote(const mojo::NullAssociatedRemote&) {}

 private:
  friend struct mojo::PendingAssociatedRemoteConverter<
      CrossVariantMojoAssociatedRemote>;

  // Note: unlike CrossVariantMojoRemote, there's no initialization ordering
  // dependency here but keep the same ordering anyway to be consistent.
  uint32_t version_;
  mojo::ScopedInterfaceEndpointHandle handle_;
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

template <typename CrossVariantBase>
struct PendingAssociatedReceiverConverter<
    blink::CrossVariantMojoAssociatedReceiver<CrossVariantBase>> {
  template <typename VariantBase>
  static PendingAssociatedReceiver<VariantBase> To(
      blink::CrossVariantMojoAssociatedReceiver<CrossVariantBase>&& in) {
    return in.handle_.is_valid()
               ? PendingAssociatedReceiver<VariantBase>(std::move(in.handle_))
               : PendingAssociatedReceiver<VariantBase>();
  }
};

template <typename CrossVariantBase>
struct PendingAssociatedRemoteConverter<
    blink::CrossVariantMojoAssociatedRemote<CrossVariantBase>> {
  template <typename VariantBase>
  static PendingAssociatedRemote<VariantBase> To(
      blink::CrossVariantMojoAssociatedRemote<CrossVariantBase>&& in) {
    return in.handle_.is_valid() ? PendingAssociatedRemote<VariantBase>(
                                       std::move(in.handle_), in.version_)
                                 : PendingAssociatedRemote<VariantBase>();
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CROSS_VARIANT_MOJO_UTIL_H_

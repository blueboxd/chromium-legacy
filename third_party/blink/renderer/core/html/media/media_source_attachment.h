// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_SOURCE_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_SOURCE_ATTACHMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fileapi/url_registry.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

class MediaSource;
class MediaSourceRegistry;

// Interface for concrete non-oilpan types to coordinate potentially
// cross-context registration, deregistration, and lookup of a MediaSource via
// the MediaSourceRegistry. Upon successful lookup, enables the extension of an
// HTMLMediaElement by the MSE API, aka attachment. This type is not managed by
// oilpan due to the potentially varying context lifetimes. Concrete
// implementations of this handle same-thread (main thread) attachments
// distinctly from cross-context (MSE-in-Worker, HTMLMediaElement in main
// thread) attachments due to the increased complexity for handling the latter.
// Concrete implementations of this interface are reference counted to ensure
// they are available potentially cross-thread and from the registry.
//
// TODO(https://crbug.com/878133): This is not yet implementing the multi-thread
// aspect.
class CORE_EXPORT MediaSourceAttachment
    : public URLRegistrable,
      public WTF::ThreadSafeRefCounted<MediaSourceAttachment> {
 public:
  // Intended to be set by the MediaSourceRegistry during its singleton
  // initialization on the main thread. Caches the pointer in |registry_|.
  static void SetRegistry(MediaSourceRegistry*);

  // Services lookup calls, expected from HTMLMediaElement during its load
  // algorithm. If |url| is not known by MediaSourceRegistry, returns nullptr.
  // Otherwise, returns the MediaSource associated with |url|.
  // TODO(https://crbug.com/878133): Change this to return the refcounted
  // attachment itself, so that further operation by HTMLMediaElement on the
  // MediaSource is moderated by the attachment instance.
  static MediaSource* LookupMediaSource(const String& url);

  // The only intended caller of this constructor is
  // URLMediaSource::createObjectUrl. The raw pointer is then adopted into a
  // scoped_refptr in MediaSourceRegistryImpl::RegisterURL.
  explicit MediaSourceAttachment(MediaSource* media_source);

  // This is called on the main thread when the URLRegistry unregisters the
  // objectURL for this attachment. It releases the strong reference to the
  // MediaSource such that GC might collect it if there is no active attachment
  // represented by other strong references.
  void Unregister();

  // URLRegistrable
  URLRegistry& Registry() const override { return *registry_; }

 private:
  friend class WTF::ThreadSafeRefCounted<MediaSourceAttachment>;
  ~MediaSourceAttachment() override;

  static URLRegistry* registry_;

  // Cache of the registered MediaSource for this initial same-thread-only
  // migration of the registrable from MediaSource to MediaSourceAttachment.
  // TODO(https://crbug.com/878133): Refactor this to be mostly internal to the
  // concrete implementations of this attachment type in modules.
  Persistent<MediaSource> registered_media_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachment);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_SOURCE_ATTACHMENT_H_

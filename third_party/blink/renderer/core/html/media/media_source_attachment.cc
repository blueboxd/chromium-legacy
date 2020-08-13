// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"

#include "third_party/blink/renderer/core/html/media/media_source.h"
#include "third_party/blink/renderer/core/html/media/media_source_registry.h"

namespace blink {

// static
URLRegistry* MediaSourceAttachment::registry_ = nullptr;

// static
void MediaSourceAttachment::SetRegistry(MediaSourceRegistry* registry) {
  DCHECK(IsMainThread());
  DCHECK(!registry_);
  registry_ = registry;
}

// static
MediaSource* MediaSourceAttachment::LookupMediaSource(const String& url) {
  // The only expected caller is an HTMLMediaElement on the main thread.
  DCHECK(IsMainThread());

  if (!registry_ || url.IsEmpty())
    return nullptr;

  // This cast is safe because the only setter of |registry_| is SetRegistry().
  MediaSourceRegistry* ms_registry =
      static_cast<MediaSourceRegistry*>(registry_);

  scoped_refptr<MediaSourceAttachment> attachment =
      ms_registry->LookupMediaSource(url);
  return attachment ? attachment->registered_media_source_.Get() : nullptr;
}

MediaSourceAttachment::MediaSourceAttachment(MediaSource* media_source)
    : registered_media_source_(media_source) {
  // For this initial implementation, construction must be on the main thread,
  // since no MSE-in-Workers implementation is yet included.
  DCHECK(IsMainThread());

  DVLOG(1) << __func__ << " media_source=" << media_source;

  // Verify that at construction time, refcounting of this object begins at
  // precisely 1.
  DCHECK(HasOneRef());
}

void MediaSourceAttachment::Unregister() {
  DVLOG(1) << __func__ << " this=" << this;

  // The only expected caller is a MediaSourceRegistryImpl on the main thread.
  DCHECK(IsMainThread());

  // Release our strong reference to the MediaSource. Note that
  // revokeObjectURL of the url associated with this attachment could commonly
  // follow this path while the MediaSource (and any attachment to an
  // HTMLMediaElement) may still be alive/active.
  DCHECK(registered_media_source_);
  registered_media_source_ = nullptr;
}

MediaSourceAttachment::~MediaSourceAttachment() = default;

}  // namespace blink

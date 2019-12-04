// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "content/public/browser/web_contents.h"

namespace paint_preview {

// A base KeyedService that serves as the Public API for Paint Previews.
// Features that want to use Paint Previews should extend this class.
// The KeyedService provides a 1:1 mapping between the service and a key or
// profile allowing each feature built on Paint Previews to reliably store
// necessary data to the right directory on disk.
//
// Implementations of this service should be created by implementing a factory
// that extends one of:
// - BrowserContextKeyedServiceFactory
// OR preferably the
// - SimpleKeyedServiceFactory
class PaintPreviewBaseService : public KeyedService {
 public:
  enum CaptureStatus {
    kOk = 0,
    kClientCreationFailed,
    kCaptureFailed,
  };

  using OnCapturedCallback =
      base::OnceCallback<void(CaptureStatus,
                              std::unique_ptr<PaintPreviewProto>)>;

  // Creates a service instance for a feature. Artifacts produced will live in
  // |profile_dir|/paint_preview/|ascii_feature_name|. Implementers of the
  // factory can also elect their factory to not construct services in the event
  // a profile |is_off_the_record|.
  PaintPreviewBaseService(const base::FilePath& profile_dir,
                          const std::string& ascii_feature_name,
                          bool is_off_the_record);
  ~PaintPreviewBaseService() override;

  // Returns the file manager for the directory associated with the service.
  FileManager* GetFileManager() { return &file_manager_; }

  // Returns whether the created service is off the record.
  bool IsOffTheRecord() const { return is_off_the_record_; }

  // The following methods both capture a Paint Preview; however, their behavior
  // and intended use is different. The first method is intended for capturing
  // full page contents. Generally, this is what you should be using for most
  // features. The second method is intended for capturing just an individual
  // RenderFrameHost and its descendents. This is intended for capturing
  // individual subframes and should be used for only a few use cases.
  //
  // NOTE: |root_dir| in the following methods should be created using
  // GetFileManager()->CreateOrGetDirectoryFor(<GURL>). However, to provide
  // flexibility in managing the lifetime of created objects and ease cleanup
  // if a capture fails the service implementation is responsible for
  // implementing this management and tracking the directories in existence.
  // Data in a directory will contain:
  // - paint_preview.pb (the metadata proto)
  // - a number of SKPs listed as <guid>.skp (one per frame)
  //
  // Captures the main frame of |web_contents| (an observer for capturing Paint
  // Previews is created for web contents if it does not exist). The capture is
  // attributed to the URL of the main frame and is stored in |root_dir|. The
  // captured area is clipped to |clip_rect| if it is non-zero. On completion
  // the status of the capture is provided via |callback|.
  void CapturePaintPreview(content::WebContents* web_contents,
                           const base::FilePath& root_dir,
                           gfx::Rect clip_rect,
                           OnCapturedCallback callback);
  // Same as above except |render_frame_host| is directly captured rather than
  // the main frame.
  void CapturePaintPreview(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           const base::FilePath& root_dir,
                           gfx::Rect clip_rect,
                           OnCapturedCallback callback);

 private:
  void OnCaptured(OnCapturedCallback callback,
                  base::UnguessableToken guid,
                  mojom::PaintPreviewStatus status,
                  std::unique_ptr<PaintPreviewProto> proto);

  FileManager file_manager_;
  bool is_off_the_record_;

  base::WeakPtrFactory<PaintPreviewBaseService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaintPreviewBaseService);
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_BASE_SERVICE_H_

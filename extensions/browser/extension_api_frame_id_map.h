// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_API_FRAME_ID_MAP_H_
#define EXTENSIONS_BROWSER_EXTENSION_API_FRAME_ID_MAP_H_

#include <list>
#include <map>
#include <memory>
#include <set>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {

// TODO(http://crbug.com/980774): Investigate if this class and
// ExtensionApiFrameIdMap are still needed.
class ExtensionApiFrameIdMapHelper {
 public:
  virtual void PopulateTabData(content::RenderFrameHost* rfh,
                               int* tab_id_out,
                               int* window_id_out) = 0;
  virtual ~ExtensionApiFrameIdMapHelper() {}
};

// Extension frame IDs are exposed through the chrome.* APIs and have the
// following characteristics:
// - The top-level frame has ID 0.
// - Any child frame has a positive ID.
// - A non-existant frame has ID -1.
// - They are only guaranteed to be unique within a tab.
// - The ID does not change during the frame's lifetime and is not re-used after
//   the frame is removed. The frame may change its current RenderFrameHost over
//   time, so multiple RenderFrameHosts may map to the same extension frame ID.
//
// This class provides a mapping from a (render_process_id, frame_routing_id)
// pair to a FrameData struct, which includes the extension's frame id (as
// described above), the parent frame id, and the tab id (the latter can be
// invalid if it's not in a tab).
//
// Unless stated otherwise, the methods can only be called on the UI thread.
//
// The non-static methods of this class use an internal cache.
// TODO(http://crbug.com/980774): This cache may not be necessary now that this
// is not accessed on IO.
class ExtensionApiFrameIdMap {
 public:
  // The data for a RenderFrame. Every RenderFrameIdKey maps to a FrameData.
  struct FrameData {
    FrameData();
    FrameData(int frame_id,
              int parent_frame_id,
              int tab_id,
              int window_id,
              GURL last_committed_main_frame_url);
    ~FrameData();

    FrameData(const FrameData&);
    FrameData& operator=(const FrameData&);

    // The extension API frame ID of the frame.
    int frame_id;

    // The extension API frame ID of the parent of the frame.
    int parent_frame_id;

    // The id of the tab that the frame is in, or -1 if the frame isn't in a
    // tab.
    int tab_id;

    // The id of the window that the frame is in, or -1 if the frame isn't in a
    // window.
    int window_id;

    // The last committed url of the main frame to which this frame belongs.
    // This ignores any same-document navigations.
    GURL last_committed_main_frame_url;

    // The pending main frame url. This is only non-empty for main frame data
    // when the main frame is ready to commit navigation but hasn't fully
    // completed the navigation yet. This ignores any same-document navigations.
    base::Optional<GURL> pending_main_frame_url;
  };

  using FrameDataCallback = base::Callback<void(const FrameData&)>;

  // An invalid extension API frame ID.
  static const int kInvalidFrameId;

  // Extension API frame ID of the top-level frame.
  static const int kTopFrameId;

  static ExtensionApiFrameIdMap* Get();

  // Get the extension API frame ID for |rfh|.
  static int GetFrameId(content::RenderFrameHost* rfh);

  // Get the extension API frame ID for |navigation_handle|.
  static int GetFrameId(content::NavigationHandle* navigation_handle);

  // Get the extension API frame ID for the parent of |rfh|.
  static int GetParentFrameId(content::RenderFrameHost* rfh);

  // Get the extension API frame ID for the parent of |navigation_handle|.
  static int GetParentFrameId(content::NavigationHandle* navigation_handle);

  // Find the current RenderFrameHost for a given WebContents and extension
  // frame ID.
  // Returns nullptr if not found.
  static content::RenderFrameHost* GetRenderFrameHostById(
      content::WebContents* web_contents,
      int frame_id);

  // Retrieves the FrameData for a given |render_process_id| and
  // |render_frame_id|. The map may be updated with the result if the map did
  // not contain the FrameData before the lookup. If a RenderFrameHost is not
  // found, then the map is not modified.
  FrameData GetFrameData(int render_process_id,
                         int render_frame_id) WARN_UNUSED_RESULT;

  // Initializes the FrameData for the given |rfh|.
  void InitializeRenderFrameData(content::RenderFrameHost* rfh);

  // Called when a render frame is deleted. Removes the FrameData mapping for
  // the given render frame.
  void OnRenderFrameDeleted(content::RenderFrameHost* rfh);

  // Updates the tab and window id for the given RenderFrameHost if necessary.
  void UpdateTabAndWindowId(int tab_id,
                            int window_id,
                            content::RenderFrameHost* rfh);

  // Called when WebContentsObserver::ReadyToCommitNavigation is dispatched for
  // a main frame.
  void OnMainFrameReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle);

  // Called when WebContentsObserver::DidFinishNavigation is dispatched for a
  // main frame.
  void OnMainFrameDidFinishNavigation(
      content::NavigationHandle* navigation_handle);

  // Returns whether frame data for |rfh| is cached.
  bool HasCachedFrameDataForTesting(content::RenderFrameHost* rfh) const;

  size_t GetFrameDataCountForTesting() const;

 protected:
  friend struct base::LazyInstanceTraitsBase<ExtensionApiFrameIdMap>;

  // A set of identifiers that uniquely identifies a RenderFrame.
  struct RenderFrameIdKey {
    RenderFrameIdKey();
    RenderFrameIdKey(int render_process_id, int frame_routing_id);

    // The process ID of the renderer that contains the RenderFrame.
    int render_process_id;

    // The routing ID of the RenderFrame.
    int frame_routing_id;

    bool operator<(const RenderFrameIdKey& other) const;
    bool operator==(const RenderFrameIdKey& other) const;
  };

  struct FrameDataCallbacks {
    FrameDataCallbacks();
    FrameDataCallbacks(const FrameDataCallbacks& other);
    ~FrameDataCallbacks();

    // This is a std::list so that iterators are not invalidated when the list
    // is modified during an iteration.
    std::list<FrameDataCallback> callbacks;

    // To avoid re-entrant processing of callbacks.
    bool is_iterating;
  };

  using FrameDataMap = std::map<RenderFrameIdKey, FrameData>;
  using FrameDataCallbacksMap = std::map<RenderFrameIdKey, FrameDataCallbacks>;

  ExtensionApiFrameIdMap();
  virtual ~ExtensionApiFrameIdMap();

  // Determines the value to be stored in |frame_data_map_| for a given key.
  // Returns empty FrameData when the corresponding RenderFrameHost is not
  // alive. This method is only called when |key| is not in |frame_data_map_|.
  // Virtual for testing.
  virtual FrameData KeyToValue(const RenderFrameIdKey& key) const;

  // Looks up the data for the given |key| and adds it to the |frame_data_map_|.
  // If |check_deleted_frames| is true, |deleted_frame_data_map_| will also be
  // used.
  FrameData LookupFrameDataOnUI(const RenderFrameIdKey& key,
                                bool check_deleted_frames);

  std::unique_ptr<ExtensionApiFrameIdMapHelper> helper_;

  // This map holds a mapping of render frame key to FrameData.
  // TODO(http://crbug.com/980774): Investigate if this is still needed.
  FrameDataMap frame_data_map_;

  // Holds mappings of render frame key to FrameData from frames that have been
  // recently deleted. These are kept for a short time so beacon requests that
  // continue after a frame is unloaded can access the FrameData.
  FrameDataMap deleted_frame_data_map_;

  // The set of pending main frame navigations for which ReadyToCommitNavigation
  // has been fired. Only used on the UI thread. This is needed to clear state
  // set up in OnMainFrameReadyToCommitNavigation for navigations which
  // eventually do not commit.
  std::set<content::NavigationHandle*> ready_to_commit_document_navigations_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionApiFrameIdMap);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_API_FRAME_ID_MAP_H_

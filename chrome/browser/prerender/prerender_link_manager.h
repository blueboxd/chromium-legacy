// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

FORWARD_DECLARE_TEST(WebViewTest, NoPrerenderer);

namespace prerender {

class PrerenderContents;
class PrerenderManager;

// PrerenderLinkManager implements the API on Link elements for all documents
// being rendered in this chrome instance.  It receives messages from the
// renderer indicating addition, cancelation and abandonment of link elements,
// and controls the PrerenderManager accordingly.
class PrerenderLinkManager : public KeyedService,
                             public PrerenderHandle::Observer {
 public:
  explicit PrerenderLinkManager(PrerenderManager* manager);
  ~PrerenderLinkManager() override;

  // A <link rel=prerender ...> element has been inserted into the document.
  // Returns true if a prerender was added.
  bool OnAddPrerender(
      int launcher_render_process_id,
      int launcher_render_view_id,
      blink::mojom::PrerenderAttributesPtr attributes,
      mojo::PendingRemote<blink::mojom::PrerenderHandleClient> handle_client,
      mojo::PendingReceiver<blink::mojom::PrerenderHandle> handle);

 private:
  friend class PrerenderBrowserTest;
  friend class PrerenderTest;
  // WebViewTest.NoPrerenderer needs to access the private IsEmpty() method.
  FRIEND_TEST_ALL_PREFIXES(::WebViewTest, NoPrerenderer);

  class PrerenderHandleProxy;
  class LinkPrerender;
  class PendingPrerenderManager;

  // A <link rel=prerender ...> element has been explicitly removed from a
  // document.
  void OnCancelPrerender(LinkPrerender* link_prerender);

  // A renderer launching <link rel=prerender ...> has navigated away from the
  // launching page, the launching renderer process has crashed, or perhaps the
  // renderer process was fast-closed when the last render view in it was
  // closed.
  void OnAbandonPrerender(LinkPrerender* link_prerender);

  bool IsEmpty() const;

  bool PrerenderIsRunningForTesting(LinkPrerender* link_prerender) const;

  // Returns a count of currently running prerenders.
  size_t CountRunningPrerenders() const;

  // Start any prerenders that can be started, respecting concurrency limits for
  // the system and per launcher.
  void StartPrerenders();

  LinkPrerender* FindByPrerenderHandle(PrerenderHandle* prerender_handle);

  // Removes |prerender| from the the prerender link manager. Deletes the
  // PrerenderHandle as needed.
  void RemovePrerender(LinkPrerender* prerender);

  // Cancels |prerender| and removes |prerender| from the prerender link
  // manager.
  void CancelPrerender(LinkPrerender* prerender);

  // Called when |launcher| is swapped in.
  void StartPendingPrerendersForLauncher(PrerenderContents* launcher);

  // Called when |launcher| is aborted.
  void CancelPendingPrerendersForLauncher(PrerenderContents* launcher);

  // From KeyedService:
  void Shutdown() override;

  // From PrerenderHandle::Observer:
  void OnPrerenderStart(PrerenderHandle* prerender_handle) override;
  void OnPrerenderStopLoading(PrerenderHandle* prerender_handle) override;
  void OnPrerenderDomContentLoaded(PrerenderHandle* prerender_handle) override;
  void OnPrerenderStop(PrerenderHandle* prerender_handle) override;
  void OnPrerenderNetworkBytesChanged(
      PrerenderHandle* prerender_handle) override;

  bool has_shutdown_;

  PrerenderManager* const manager_;

  // All prerenders known to this PrerenderLinkManager. Insertions are always
  // made at the back, so the oldest prerender is at the front, and the youngest
  // at the back. Using std::unique_ptr<> here as LinkPrerender is not copyable.
  std::list<std::unique_ptr<LinkPrerender>> prerenders_;

  // Helper object to manage prerenders which are launched by other prerenders
  // and must be deferred until the launcher is swapped in.
  std::unique_ptr<PendingPrerenderManager> pending_prerender_manager_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderLinkManager);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_LINK_MANAGER_H_

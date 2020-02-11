// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_BASE_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_BASE_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_view.h"
#include "url/gurl.h"

namespace content {
class SiteInstance;
}  // namespace content

namespace chromecast {

class CastWebService;
class RendererPrelauncher;

// Common logic for CastWebView implementations.
class CastWebViewBase : public CastWebView {
 public:
  CastWebViewBase(const CreateParams& create_params,
                  CastWebService* web_service);
  CastWebViewBase(const CastWebViewBase&) = delete;
  CastWebViewBase& operator=(const CastWebViewBase&) = delete;
  ~CastWebViewBase() override;

  scoped_refptr<content::SiteInstance> site_instance() const {
    return site_instance_;
  }

  // CastWebView implementation (partial):
  void ForceClose() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::TimeDelta shutdown_delay() const override;

 protected:
  base::WeakPtr<Delegate> delegate_;
  CastWebService* const web_service_;

 private:
  base::TimeDelta shutdown_delay_;
  const RendererPool renderer_pool_;
  const GURL prelaunch_url_;

  std::unique_ptr<RendererPrelauncher> renderer_prelauncher_;
  scoped_refptr<content::SiteInstance> site_instance_;

  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_BASE_H_

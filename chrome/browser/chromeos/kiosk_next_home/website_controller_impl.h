// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_WEBSITE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_WEBSITE_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/kiosk_next_home/mojom/website_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

class GURL;

namespace chromeos {
namespace kiosk_next_home {

class WebsiteControllerImpl : public mojom::WebsiteController {
 public:
  explicit WebsiteControllerImpl(mojom::WebsiteControllerRequest request);
  ~WebsiteControllerImpl() override;

  // Launches the website using KioskNextBrowserFactory.
  void LaunchKioskNextWebsite(const GURL& url) override;

  // Launches the url in normal tabbed browser window.
  void LaunchWebsite(const GURL& url) override;

 private:
  mojo::Binding<mojom::WebsiteController> binding_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteControllerImpl);
};

}  // namespace kiosk_next_home
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_WEBSITE_CONTROLLER_IMPL_H_

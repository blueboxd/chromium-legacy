// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_COLOR_CHOOSER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_COLOR_CHOOSER_MAC_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "components/remote_cocoa/common/color_panel.mojom.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/binding.h"

class ColorChooserMac : public content::ColorChooser,
                        public remote_cocoa::mojom::ColorPanelHost {
 public:
  // Returns a ColorChooserMac instance owned by the ColorChooserMac class -
  // call End() when done to free it. Each call to Open() returns a new
  // instance after freeing the previous one (i.e. it does not reuse the
  // previous instance even if it still exists).
  static ColorChooserMac* Open(content::WebContents* web_contents,
                               SkColor initial_color);

  // content::ColorChooser.
  void SetSelectedColor(SkColor color) override;
  void End() override;

 private:
  // remote_cocoa::mojom::ColorPanelHost.
  void DidChooseColorInColorPanel(SkColor color) override;
  void DidCloseColorPanel() override;

  ColorChooserMac(content::WebContents* tab, SkColor initial_color);

  ~ColorChooserMac() override;

  // The web contents invoking the color chooser.  No ownership because it will
  // outlive this class.
  content::WebContents* web_contents_;

  remote_cocoa::mojom::ColorPanelPtr mojo_panel_ptr_;
  mojo::Binding<remote_cocoa::mojom::ColorPanelHost> mojo_host_binding_;
  DISALLOW_COPY_AND_ASSIGN(ColorChooserMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_COLOR_CHOOSER_MAC_H_

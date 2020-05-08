// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"

namespace extensions {

namespace {

using InfoBars = std::map<std::string, ExtensionDevToolsInfoBar*>;
base::LazyInstance<InfoBars>::Leaky g_infobars = LAZY_INSTANCE_INITIALIZER;

// The InfoBarDelegate that ExtensionDevToolsInfoBar shows.
class ExtensionDevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ExtensionDevToolsInfoBarDelegate(base::OnceClosure destroyed_callback,
                                   const std::string& extension_name);
  ExtensionDevToolsInfoBarDelegate(const ExtensionDevToolsInfoBarDelegate&) =
      delete;
  ExtensionDevToolsInfoBarDelegate& operator=(
      const ExtensionDevToolsInfoBarDelegate&) = delete;
  ~ExtensionDevToolsInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  gfx::ElideBehavior GetMessageElideBehavior() const override;
  int GetButtons() const override;

 private:
  const base::string16 extension_name_;
  base::OnceClosure destroyed_callback_;
};

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    base::OnceClosure destroyed_callback,
    const std::string& extension_name)
    : extension_name_(base::UTF8ToUTF16(extension_name)),
      destroyed_callback_(std::move(destroyed_callback)) {}

ExtensionDevToolsInfoBarDelegate::~ExtensionDevToolsInfoBarDelegate() {
  std::move(destroyed_callback_).Run();
}

infobars::InfoBarDelegate::InfoBarIdentifier
ExtensionDevToolsInfoBarDelegate::GetIdentifier() const {
  return EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE;
}

bool ExtensionDevToolsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

base::string16 ExtensionDevToolsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL,
                                    extension_name_);
}

gfx::ElideBehavior ExtensionDevToolsInfoBarDelegate::GetMessageElideBehavior()
    const {
  // The important part of the message text above is at the end:
  // "... is debugging the browser". If the extension name is very long,
  // we'd rather truncate it instead. See https://crbug.com/823194.
  return gfx::ELIDE_HEAD;
}

int ExtensionDevToolsInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

}  // namespace

// static
std::unique_ptr<ExtensionDevToolsInfoBar::CallbackList::Subscription>
ExtensionDevToolsInfoBar::Create(const std::string& extension_id,
                                 const std::string& extension_name,
                                 base::OnceClosure destroyed_callback) {
  auto it = g_infobars.Get().find(extension_id);
  ExtensionDevToolsInfoBar* infobar = nullptr;
  if (it != g_infobars.Get().end())
    infobar = it->second;
  else
    infobar = new ExtensionDevToolsInfoBar(extension_id, extension_name);
  return infobar->RegisterDestroyedCallback(std::move(destroyed_callback));
}

ExtensionDevToolsInfoBar::ExtensionDevToolsInfoBar(
    std::string extension_id,
    const std::string& extension_name)
    : extension_id_(std::move(extension_id)) {
  g_infobars.Get()[extension_id_] = this;

  // This class outlives the delegate, so it's safe to pass Unretained(this).
  auto delegate = std::make_unique<ExtensionDevToolsInfoBarDelegate>(
      base::BindOnce(&ExtensionDevToolsInfoBar::InfoBarDestroyed,
                     base::Unretained(this)),
      extension_name);
  GlobalConfirmInfoBar::Show(std::move(delegate));
}

ExtensionDevToolsInfoBar::~ExtensionDevToolsInfoBar() = default;

std::unique_ptr<ExtensionDevToolsInfoBar::CallbackList::Subscription>
ExtensionDevToolsInfoBar::RegisterDestroyedCallback(
    base::OnceClosure destroyed_callback) {
  return callback_list_.Add(std::move(destroyed_callback));
}

void ExtensionDevToolsInfoBar::InfoBarDestroyed() {
  callback_list_.Notify();
  g_infobars.Get().erase(extension_id_);
  delete this;
}

}  // namespace extensions

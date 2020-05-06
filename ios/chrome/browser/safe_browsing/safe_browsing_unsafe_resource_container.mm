// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::UnsafeResource;

#pragma mark - UnsafeSubresourceContainer

namespace {
// Helper object storing unsafe subresources for a NavigationItem.
class UnsafeSubresourceContainer : public base::SupportsUserData::Data {
 public:
  ~UnsafeSubresourceContainer() override = default;

  // Lazily instantiates and returns the UnsafeSubresourceContainer for |item|.
  static UnsafeSubresourceContainer* FromNavigationItem(
      web::NavigationItem* item) {
    DCHECK(item);
    static void* kUserDataKey = &kUserDataKey;
    UnsafeSubresourceContainer* stack =
        static_cast<UnsafeSubresourceContainer*>(
            item->GetUserData(kUserDataKey));
    if (!stack) {
      auto new_stack = base::WrapUnique(new UnsafeSubresourceContainer());
      stack = new_stack.get();
      item->SetUserData(kUserDataKey, std::move(new_stack));
    }
    return stack;
  }

  // Returns a pointer to unsafe resource, or null if one is not stored.
  const security_interstitials::UnsafeResource* GetUnsafeResource() const {
    return unsafe_subresource_.get();
  }

  // Stores a copy of |subresource| in the container.
  void StoreUnsafeSubresource(std::unique_ptr<UnsafeResource> subresource) {
    unsafe_subresource_ = std::move(subresource);
  }

  // Returns the unsafe subresource, transferring ownership to the caller.
  // Returns null if thre is no subresource in the container.
  std::unique_ptr<UnsafeResource> ReleaseUnsafeSubresource() {
    return std::move(unsafe_subresource_);
  }

 private:
  UnsafeSubresourceContainer() = default;

  std::unique_ptr<security_interstitials::UnsafeResource> unsafe_subresource_;
};
}

#pragma mark - SafeBrowsingUnsafeResourceContainer

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingUnsafeResourceContainer)

SafeBrowsingUnsafeResourceContainer::SafeBrowsingUnsafeResourceContainer(
    web::WebState* web_state)
    : web_state_(web_state) {}

SafeBrowsingUnsafeResourceContainer::SafeBrowsingUnsafeResourceContainer(
    SafeBrowsingUnsafeResourceContainer&& other) = default;

SafeBrowsingUnsafeResourceContainer&
SafeBrowsingUnsafeResourceContainer::operator=(
    SafeBrowsingUnsafeResourceContainer&& other) = default;

SafeBrowsingUnsafeResourceContainer::~SafeBrowsingUnsafeResourceContainer() =
    default;

void SafeBrowsingUnsafeResourceContainer::StoreUnsafeResource(
    const UnsafeResource& resource) {
  DCHECK(!resource.web_state_getter.is_null() &&
         resource.web_state_getter.Run() == web_state_);
  // Store a copy of the resource, but reset the callback to prevent misuse.
  std::unique_ptr<UnsafeResource> resource_copy =
      std::make_unique<UnsafeResource>(resource);
  resource_copy->callback = UnsafeResource::UrlCheckCallback();
  if (resource.resource_type == safe_browsing::ResourceType::kMainFrame) {
    // For main frame navigations, the copy is stored in
    // |main_frame_unsafe_resource_| it corresponds with the pending
    // NavigationItem, which is discarded when the navigation is cancelled.
    DCHECK(!GetMainFrameUnsafeResource());
    main_frame_unsafe_resource_ = std::move(resource_copy);
  } else {
    // Unsafe subresources are caused by loads triggered by the committed main
    // frame navigation.  These are associated with the NavigationItem stack so
    // that they persist past reloads.
    web::NavigationItem* item =
        web_state_->GetNavigationManager()->GetLastCommittedItem();
    DCHECK(!GetSubFrameUnsafeResource(item));
    UnsafeSubresourceContainer::FromNavigationItem(item)
        ->StoreUnsafeSubresource(std::move(resource_copy));
  }
}

const security_interstitials::UnsafeResource*
SafeBrowsingUnsafeResourceContainer::GetMainFrameUnsafeResource() const {
  return main_frame_unsafe_resource_.get();
}

std::unique_ptr<UnsafeResource>
SafeBrowsingUnsafeResourceContainer::ReleaseMainFrameUnsafeResource() {
  return std::move(main_frame_unsafe_resource_);
}

const security_interstitials::UnsafeResource*
SafeBrowsingUnsafeResourceContainer::GetSubFrameUnsafeResource(
    web::NavigationItem* item) const {
  return UnsafeSubresourceContainer::FromNavigationItem(item)
      ->GetUnsafeResource();
}

std::unique_ptr<security_interstitials::UnsafeResource>
SafeBrowsingUnsafeResourceContainer::ReleaseSubFrameUnsafeResource(
    web::NavigationItem* item) {
  return UnsafeSubresourceContainer::FromNavigationItem(item)
      ->ReleaseUnsafeSubresource();
}

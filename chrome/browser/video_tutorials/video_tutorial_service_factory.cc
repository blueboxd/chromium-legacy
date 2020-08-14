// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/video_tutorial_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/video_tutorials/tutorial_factory_helper.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/keyed_service/core/simple_dependency_manager.h"

namespace video_tutorials {

// static
VideoTutorialServiceFactory* VideoTutorialServiceFactory::GetInstance() {
  return base::Singleton<VideoTutorialServiceFactory>::get();
}

// static
VideoTutorialService* VideoTutorialServiceFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<VideoTutorialService*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

VideoTutorialServiceFactory::VideoTutorialServiceFactory()
    : SimpleKeyedServiceFactory("VideoTutorialService",
                                SimpleDependencyManager::GetInstance()) {
  DependsOn(ImageFetcherServiceFactory::GetInstance());
  DependsOn(background_task::BackgroundTaskSchedulerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
VideoTutorialServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return CreateVideoTutorialService();
}

}  // namespace video_tutorials

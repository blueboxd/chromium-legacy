# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CHROME=google-chrome

PROD_ICONS = \
	src/images/camera_app_icons_128.png \
	src/images/camera_app_icons_48.png \

CANARY_ICONS = \
	utils/canary/images/camera_app_icons_128.png \
	utils/canary/images/camera_app_icons_48.png \

DEV_ICONS = \
	utils/dev/images/camera_app_icons_128.png \
	utils/dev/images/camera_app_icons_48.png \

# Include all resources of the Camera App to be copied to the target package,
# but without the manifest files and app icons (to be added later).
RESOURCES = \
	src/_locales/am/messages.json \
	src/_locales/ar/messages.json \
	src/_locales/bg/messages.json \
	src/_locales/bn/messages.json \
	src/_locales/ca/messages.json \
	src/_locales/cs/messages.json \
	src/_locales/da/messages.json \
	src/_locales/de/messages.json \
	src/_locales/el/messages.json \
	src/_locales/en/messages.json \
	src/_locales/en_GB/messages.json \
	src/_locales/es/messages.json \
	src/_locales/es_419/messages.json \
	src/_locales/et/messages.json \
	src/_locales/fa/messages.json \
	src/_locales/fi/messages.json \
	src/_locales/fil/messages.json \
	src/_locales/fr/messages.json \
	src/_locales/gu/messages.json \
	src/_locales/he/messages.json \
	src/_locales/hi/messages.json \
	src/_locales/hr/messages.json \
	src/_locales/hu/messages.json \
	src/_locales/id/messages.json \
	src/_locales/it/messages.json \
	src/_locales/ja/messages.json \
	src/_locales/kn/messages.json \
	src/_locales/ko/messages.json \
	src/_locales/lt/messages.json \
	src/_locales/lv/messages.json \
	src/_locales/ml/messages.json \
	src/_locales/mr/messages.json \
	src/_locales/ms/messages.json \
	src/_locales/nl/messages.json \
	src/_locales/no/messages.json \
	src/_locales/pl/messages.json \
	src/_locales/pt_BR/messages.json \
	src/_locales/pt_PT/messages.json \
	src/_locales/ro/messages.json \
	src/_locales/ru/messages.json \
	src/_locales/sk/messages.json \
	src/_locales/sl/messages.json \
	src/_locales/sr/messages.json \
	src/_locales/sv/messages.json \
	src/_locales/sw/messages.json \
	src/_locales/ta/messages.json \
	src/_locales/te/messages.json \
	src/_locales/th/messages.json \
	src/_locales/tr/messages.json \
	src/_locales/uk/messages.json \
	src/_locales/vi/messages.json \
	src/_locales/zh_CN/messages.json \
	src/_locales/zh_TW/messages.json \
	src/css/main.css \
	src/images/browser_button_back.svg \
	src/images/browser_button_delete.svg \
	src/images/browser_button_export.svg \
	src/images/browser_button_print.svg \
	src/images/camera_button_fps_30.svg \
	src/images/camera_button_fps_60.svg \
	src/images/camera_button_grid_off.svg \
	src/images/camera_button_grid_on.svg \
	src/images/camera_button_mic_off.svg \
	src/images/camera_button_mic_on.svg \
	src/images/camera_button_mirror_off.svg \
	src/images/camera_button_mirror_on.svg \
	src/images/camera_button_settings.svg \
	src/images/camera_button_switch_device.svg \
	src/images/camera_button_switch_photo.svg \
	src/images/camera_button_switch_video.svg \
	src/images/camera_button_timer_off.svg \
	src/images/camera_button_timer_on_10s.svg \
	src/images/camera_button_timer_on_3s.svg \
	src/images/camera_focus_aim.svg \
	src/images/camera_intro_banner_close.svg \
	src/images/camera_intro_banner_icon.svg \
	src/images/camera_mode_photo.svg \
	src/images/camera_mode_portrait.svg \
	src/images/camera_mode_square.svg \
	src/images/camera_mode_video.svg \
	src/images/camera_shutter_photo_start.svg \
	src/images/camera_shutter_photo_start_active.svg \
	src/images/camera_shutter_photo_start_hover.svg \
	src/images/camera_shutter_photo_stop.svg \
	src/images/camera_shutter_photo_stop_hover.svg \
	src/images/camera_shutter_video_start.svg \
	src/images/camera_shutter_video_start_hover.svg \
	src/images/camera_shutter_video_stop.svg \
	src/images/camera_shutter_video_stop_hover.svg \
	src/images/settings_button_back.svg \
	src/images/settings_button_expand.svg \
	src/images/settings_feedback.svg \
	src/images/settings_grid_type.svg \
	src/images/settings_help.svg \
	src/images/settings_resolution.svg \
	src/images/settings_timer_duration.svg \
	src/images/spinner.svg \
	src/js/background.js \
	src/js/browser_proxy/browser_proxy.js \
	src/js/device/camera3_device_info.js \
	src/js/device/constraints_preferrer.js \
	src/js/device/device_info_updater.js \
	src/js/gallerybutton.js \
	src/js/google-analytics-bundle.js \
	src/js/main.js \
	src/js/metrics.js \
	src/js/models/filenamer.js \
	src/js/models/filesystem.js \
	src/js/models/gallery.js \
	src/js/mojo/imagecapture.js \
	src/js/nav.js \
	src/js/resolution_event_broker.js \
	src/js/scrollbar.js \
	src/js/sound.js \
	src/js/state.js \
	src/js/toast.js \
	src/js/tooltip.js \
	src/js/util.js \
	src/js/views/browser.js \
	src/js/views/camera.js \
	src/js/views/camera/layout.js \
	src/js/views/camera/modes.js \
	src/js/views/camera/options.js \
	src/js/views/camera/preview.js \
	src/js/views/camera/recordtime.js \
	src/js/views/camera/timertick.js \
	src/js/views/dialog.js \
	src/js/views/gallery_base.js \
	src/js/views/settings.js \
	src/js/views/view.js \
	src/js/views/warning.js \
	src/sounds/record_end.ogg \
	src/sounds/record_start.ogg \
	src/sounds/shutter.ogg \
	src/sounds/tick.ogg \
	src/views/main.html \
	src/LICENSE \

# Resources for the Prod build.
RESOURCES_PROD = $(RESOURCES) $(PROD_ICONS)

# Resources for the Canary build.
RESOURCES_CANARY = $(RESOURCES) $(CANARY_ICONS)

# Resources for the Dev build.
RESOURCES_DEV = $(RESOURCES) $(DEV_ICONS)

# Path for the Camera resources. Relative, with a trailing slash.
SRC_PATH=src/

# Path for Canary-specific utility files. Relative, with a trailing slash.
CANARY_UTILS_PATH=utils/canary/

# Path for Dev-specific utility files. Relative, with a trailing slash.
DEV_UTILS_PATH=utils/dev/

# Manifest file for the camera.crx package.
SRC_MANIFEST=src/manifest.json

# Builds camera.crx
all: build/camera.crx

# Builds the release version.
build/camera: $(RESOURCES_PROD) $(SRC_MANIFEST)
	mkdir -p build/camera
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(RESOURCES_PROD)) ../build/camera
	cp $(SRC_MANIFEST) build/camera/manifest.json

# Builds the camera.crx package.
build/camera.crx: build/camera
	$(CHROME) --pack-extension=build/camera \
	  --pack-extension-key=dev-keys/camera.pem

# Alias for build/camera.crx.
camera: build/camera.crx

# Canary build
canary: build/camera-canary

# Dev build
dev: build/camera-dev.crx

# Builds the canary version.
build/camera-canary: $(RESOURCES_CANARY) $(SRC_MANIFEST)
	mkdir -p build/camera-canary
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(RESOURCES)) ../build/camera-canary
	cp -r $(CANARY_UTILS_PATH)* build/camera-canary
	utils/generate_manifest.py --canary

# Builds the dev version.
build/camera-dev: $(RESOURCES_DEV) $(SRC_MANIFEST)
	mkdir -p build/camera-dev
	cd $(SRC_PATH); cp --parents $(patsubst $(SRC_PATH)%, %, \
	  $(RESOURCES)) ../build/camera-dev
	cp -r $(DEV_UTILS_PATH)* build/camera-dev
	utils/generate_manifest.py --dev

# Builds the camera-dev.crx package.
build/camera-dev.crx: build/camera-dev
	$(CHROME) --pack-extension=build/camera-dev \
	  --pack-extension-key=dev-keys/camera.pem

# Cleans the workspace from build files files.
clean:
	rm -rf build

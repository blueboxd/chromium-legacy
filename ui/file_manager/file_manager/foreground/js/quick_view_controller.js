// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for QuickView.
 */
class QuickViewController {
  /**
   * This should be initialized with |init_| method.
   *
   * @param {!MetadataModel} metadataModel File system metadata.
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!ListContainer} listContainer
   * @param {!cr.ui.MultiMenuButton} selectionMenuButton
   * @param {!QuickViewModel} quickViewModel
   * @param {!TaskController} taskController
   * @param {!FileListSelectionModel} fileListSelectionModel
   * @param {!QuickViewUma} quickViewUma
   * @param {!MetadataBoxController} metadataBoxController
   * @param {DialogType} dialogType
   * @param {!VolumeManager} volumeManager
   * @param {!HTMLElement} dialogDom
   */
  constructor(
      metadataModel, selectionHandler, listContainer, selectionMenuButton,
      quickViewModel, taskController, fileListSelectionModel, quickViewUma,
      metadataBoxController, dialogType, volumeManager, dialogDom) {
    /** @private {?FilesQuickView} */
    this.quickView_ = null;

    /** @private @const {!FileSelectionHandler} */
    this.selectionHandler_ = selectionHandler;

    /** @private @const {!ListContainer} */
    this.listContainer_ = listContainer;

    /** @private @const{!QuickViewModel} */
    this.quickViewModel_ = quickViewModel;

    /** @private @const {!QuickViewUma} */
    this.quickViewUma_ = quickViewUma;

    /** @private @const {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const {!TaskController} */
    this.taskController_ = taskController;

    /** @private @const {!FileListSelectionModel} */
    this.fileListSelectionModel_ = fileListSelectionModel;

    /** @private @const {!MetadataBoxController} */
    this.metadataBoxController_ = metadataBoxController;

    /** @private @const {DialogType} */
    this.dialogType_ = dialogType;

    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /**
     * Current selection of selectionHandler.
     * @private {!Array<!FileEntry>}
     */
    this.entries_ = [];

    /**
     * The tasks for the entry currently shown in Quick View.
     * @private {?FileTasks}
     */
    this.tasks_ = null;

    /**
     * The current selection index of this.entries_.
     * @private {number}
     */
    this.currentSelection_ = 0;

    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE,
        this.onFileSelectionChanged_.bind(this));
    this.listContainer_.element.addEventListener(
        'keydown', this.onKeyDownToOpen_.bind(this));
    dialogDom.addEventListener('command', event => {
      // Selection menu command can be triggered with focus outside of file list
      // or button e.g.: from the directory tree.
      if (event.command.id === 'get-info') {
        this.display_(QuickViewUma.WayToOpen.SELECTION_MENU);
      }
    });
    this.listContainer_.element.addEventListener('command', event => {
      if (event.command.id === 'get-info') {
        this.display_(QuickViewUma.WayToOpen.CONTEXT_MENU);
      }
    });
    selectionMenuButton.addEventListener('command', event => {
      if (event.command.id === 'get-info') {
        this.display_(QuickViewUma.WayToOpen.SELECTION_MENU);
      }
    });
  }

  /**
   * Initialize the controller with quick view which will be lazily loaded.
   * @param {!FilesQuickView} quickView
   * @private
   */
  init_(quickView) {
    this.quickView_ = quickView;
    this.quickView_.isModal = DialogType.isModal(this.dialogType_);

    this.metadataBoxController_.init(quickView);

    document.body.addEventListener(
        'keydown', this.onQuickViewKeyDown_.bind(this));
    this.quickView_.addEventListener('close', () => {
      this.listContainer_.focus();
    });
    this.quickView_.onOpenInNewButtonTap =
        this.onOpenInNewButtonTap_.bind(this);

    const toolTip = this.quickView_.$$('files-tooltip');
    const elems =
        this.quickView_.$$('#toolbar').querySelectorAll('[has-tooltip]');
    toolTip.addTargets(elems);
  }

  /**
   * Create quick view element.
   * @return Promise<!FilesQuickView>
   * @private
   */
  createQuickView_() {
    return new Promise((resolve, reject) => {
      Polymer.Base.importHref(constants.FILES_QUICK_VIEW_HTML, () => {
        const quickView = document.querySelector('#quick-view');
        resolve(quickView);
      }, reject);
    });
  }

  /**
   * Handles open-in-new button tap.
   *
   * @param {!Event} event A button click event.
   * @private
   */
  onOpenInNewButtonTap_(event) {
    this.tasks_ && this.tasks_.executeDefault();
    this.quickView_.close();
  }

  /**
   * Handles key event on listContainer if it's relevant to quick view.
   *
   * @param {!Event} event A keyboard event.
   * @private
   */
  onKeyDownToOpen_(event) {
    if (event.key === ' ') {
      event.preventDefault();
      event.stopImmediatePropagation();
      this.display_(QuickViewUma.WayToOpen.SPACE_KEY);
    }
  }

  /**
   * Handles key event on quick view.
   *
   * @param {!Event} event A keyboard event.
   * @private
   */
  onQuickViewKeyDown_(event) {
    if (this.quickView_.isOpened()) {
      let index;
      switch (event.key) {
        case ' ':
        case 'Escape':
          event.preventDefault();
          // Prevent the open dialog from closing.
          event.stopImmediatePropagation();
          this.quickView_.close();
          break;
        case 'ArrowRight':
        case 'ArrowDown':
          if (this.fileListSelectionModel_.getCheckSelectMode()) {
            this.changeCheckSelectModeSelection_(true);
          } else {
            this.changeSingleSelectModeSelection_(true);
          }
          break;
        case 'ArrowLeft':
        case 'ArrowUp':
          if (this.fileListSelectionModel_.getCheckSelectMode()) {
            this.changeCheckSelectModeSelection_();
          } else {
            this.changeSingleSelectModeSelection_();
          }
          break;
      }
    }
  }

  /**
   * Changes the currently selected entry when in single-select mode.  Sets
   * the models |selectedIndex| to indirectly trigger onFileSelectionChanged_
   * and populate |this.entries_|.
   *
   * @param {boolean} down True if user pressed down arrow, false if up.
   * @private
   */
  changeSingleSelectModeSelection_(down = false) {
    let index;

    if (down) {
      index = this.fileListSelectionModel_.selectedIndex + 1;
      if (index >= this.fileListSelectionModel_.length) {
        index = 0;
      }
    } else {
      index = this.fileListSelectionModel_.selectedIndex - 1;
      if (index < 0) {
        index = this.fileListSelectionModel_.length - 1;
      }
    }

    this.fileListSelectionModel_.selectedIndex = index;
  }

  /**
   * Changes the currently selected entry when in multi-select mode (file
   * list calls this "check-select" mode).
   *
   * @param {boolean} down True if user pressed down arrow, false if up.
   * @private
   */
  changeCheckSelectModeSelection_(down = false) {
    if (down) {
      this.currentSelection_ = this.currentSelection_ + 1;
      if (this.currentSelection_ >=
          this.fileListSelectionModel_.selectedIndexes.length) {
        this.currentSelection_ = 0;
      }
    } else {
      this.currentSelection_ = this.currentSelection_ - 1;
      if (this.currentSelection_ < 0) {
        this.currentSelection_ =
            this.fileListSelectionModel_.selectedIndexes.length - 1;
      }
    }

    this.onFileSelectionChanged_(null);
  }


  /**
   * Display quick view.
   *
   * @param {QuickViewUma.WayToOpen} wayToOpen The open quick view trigger.
   * @private
   */
  display_(wayToOpen) {
    // On opening Quick View, always reset the current selection index.
    this.currentSelection_ = 0;

    this.updateQuickView_().then(() => {
      if (!this.quickView_.isOpened()) {
        this.quickView_.open();
        this.quickViewUma_.onOpened(this.entries_[0], wayToOpen);
      }
    });
  }

  /**
   * Update quick view on file selection change.
   *
   * @param {?Event} event an Event whose target is FileSelectionHandler.
   * @private
   */
  onFileSelectionChanged_(event) {
    if (event) {
      this.entries_ = event.target.selection.entries;
    }

    if (this.quickView_ && this.quickView_.isOpened()) {
      assert(this.entries_.length > 0);
      const entry = this.entries_[this.currentSelection_];

      if (!util.isSameEntry(entry, this.quickViewModel_.getSelectedEntry())) {
        this.updateQuickView_();
      }
    }
  }

  /**
   * @param {!FileEntry} entry
   * @return {!Promise<!Array<!chrome.fileManagerPrivate.FileTask>>}
   * @private
   */
  getAvailableTasks_(entry) {
    return this.taskController_.getEntryFileTasks(entry).then(tasks => {
      this.tasks_ = tasks;
      return tasks.getTaskItems();
    });
  }

  /**
   * Update quick view using current entries.
   *
   * @return {!Promise} Promise fulfilled after quick view is updated.
   * @private
   */
  updateQuickView_() {
    if (!this.quickView_) {
      return this.createQuickView_()
          .then(this.init_.bind(this))
          .then(this.updateQuickView_.bind(this))
          .catch(console.error);
    }

    assert(this.entries_.length > 0);
    const entry = this.entries_[this.currentSelection_];
    this.quickViewModel_.setSelectedEntry(entry);

    requestIdleCallback(() => {
      this.quickViewUma_.onEntryChanged(entry);
    });

    return Promise
        .all([
          this.metadataModel_.get([entry], ['thumbnailUrl']),
          this.getAvailableTasks_(entry)
        ])
        .then(values => {
          const items = (/**@type{Array<MetadataItem>}*/ (values[0]));
          const tasks =
              (/**@type{!Array<!chrome.fileManagerPrivate.FileTask>}*/ (
                  values[1]));
          return this.onMetadataLoaded_(entry, items, tasks);
        })
        .catch(error => {
          if (error) {
            console.error(error.stack || error);
          }
        });
  }

  /**
   * Update quick view for |entry| from its loaded metadata and tasks.
   *
   * Note: fast-typing users can change the active selection while the |entry|
   * metadata and tasks were being async fetched. Bail out in that case.
   *
   * @param {!FileEntry} entry
   * @param {Array<MetadataItem>} items
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   * @private
   */
  onMetadataLoaded_(entry, items, tasks) {
    return this.getQuickViewParameters_(entry, items, tasks).then(params => {
      if (this.quickViewModel_.getSelectedEntry() != entry) {
        return;  // Bail: there's no point drawing a stale selection.
      }

      this.quickView_.setProperties({
        type: params.type || '',
        subtype: params.subtype || '',
        filePath: params.filePath || '',
        hasTask: params.hasTask || false,
        contentUrl: params.contentUrl || '',
        videoPoster: params.videoPoster || '',
        audioArtwork: params.audioArtwork || '',
        autoplay: params.autoplay || false,
        browsable: params.browsable || false,
      });
    });
  }

  /**
   * @param {!FileEntry} entry
   * @param {Array<MetadataItem>} items
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   * @return !Promise<!QuickViewParams>
   *
   * @private
   */
  getQuickViewParameters_(entry, items, tasks) {
    const item = items[0];
    const typeInfo = FileType.getType(entry);
    const type = typeInfo.type;
    const locationInfo = this.volumeManager_.getLocationInfo(entry);
    const label = util.getEntryLabel(locationInfo, entry);

    /** @type {!QuickViewParams} */
    const params = {
      type: type,
      subtype: typeInfo.subtype,
      filePath: label,
      hasTask: tasks.length > 0,
    };

    const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
    const localFile = volumeInfo &&
        QuickViewController.LOCAL_VOLUME_TYPES_.indexOf(
            volumeInfo.volumeType) >= 0;

    if (!localFile) {
      // Drive files: fetch their thumbnail if there is one.
      if (item.thumbnailUrl) {
        return this.loadThumbnailFromDrive_(item.thumbnailUrl).then(result => {
          if (result.status === LoadImageResponseStatus.SUCCESS) {
            if (params.type == 'video') {
              params.videoPoster = result.data;
            } else if (params.type == 'image') {
              params.contentUrl = result.data;
            } else {
              // TODO(sashab): Rather than re-use 'image', create a new type
              // here, e.g. 'thumbnail'.
              params.type = 'image';
              params.contentUrl = result.data;
            }
          }
          return params;
        });
      }

      // We ask user to open it with external app.
      return Promise.resolve(params);
    }

    if (type === 'raw') {
      // RAW files: fetch their ImageLoader thumbnail.
      return this.loadRawFileThumbnailFromImageLoader_(entry)
          .then(result => {
            if (result.status === LoadImageResponseStatus.SUCCESS) {
              params.contentUrl = result.data;
              params.type = 'image';
            }
            return params;
          })
          .catch(e => {
            console.error(e);
            return params;
          });
    }

    if (type === '.folder') {
      return Promise.resolve(params);
    }

    return new Promise((resolve, reject) => {
             entry.file(resolve, reject);
           })
        .then(file => {
          switch (type) {
            case 'image':
              if (QuickViewController.UNSUPPORTED_IMAGE_SUBTYPES_.indexOf(
                      typeInfo.subtype) !== -1) {
                params.contentUrl = '';
              } else {
                params.contentUrl = URL.createObjectURL(file);
              }
              return params;
            case 'video':
              params.contentUrl = URL.createObjectURL(file);
              params.autoplay = true;
              if (item.thumbnailUrl) {
                params.videoPoster = item.thumbnailUrl;
              }
              return params;
            case 'audio':
              params.contentUrl = URL.createObjectURL(file);
              params.autoplay = true;
              return this.metadataModel_.get([entry], ['contentThumbnailUrl'])
                  .then(items => {
                    const item = items[0];
                    if (item.contentThumbnailUrl) {
                      params.audioArtwork = item.contentThumbnailUrl;
                    }
                    return params;
                  });
            case 'document':
              if (typeInfo.subtype === 'HTML') {
                params.contentUrl = URL.createObjectURL(file);
                return params;
              } else {
                break;
              }
          }
          const browsable = tasks.some(task => {
            return ['view-in-browser', 'view-pdf'].includes(
                task.taskId.split('|')[2]);
          });
          params.browsable = browsable;
          params.contentUrl = browsable ? URL.createObjectURL(file) : '';
          if (params.subtype == 'PDF') {
            params.contentUrl += '#view=FitH';
          }
          return params;
        })
        .catch(e => {
          console.error(e);
          return params;
        });
  }

  /**
   * Loads a thumbnail from Drive.
   *
   * @param {string} url Thumbnail url
   * @return Promise<!LoadImageResponse>
   * @private
   */
  loadThumbnailFromDrive_(url) {
    return new Promise(resolve => {
      ImageLoaderClient.getInstance().load(
          LoadImageRequest.createForUrl(url), resolve);
    });
  }

  /**
   * Loads a RAW image thumbnail from ImageLoader. Resolve the file entry first
   * to get its |lastModified| time. ImageLoaderClient uses that to work out if
   * its cached data for |entry| is up-to-date or otherwise call ImageLoader to
   * refresh the cached |entry| data with the most recent data.
   *
   * @param {!Entry} entry The RAW file entry.
   * @return Promise<!LoadImageResponse>
   * @private
   */
  loadRawFileThumbnailFromImageLoader_(entry) {
    return new Promise((resolve, reject) => {
      entry.file(function requestFileThumbnail(file) {
        const request = LoadImageRequest.createForUrl(entry.toURL());
        request.maxWidth = ThumbnailLoader.THUMBNAIL_MAX_WIDTH;
        request.maxHeight = ThumbnailLoader.THUMBNAIL_MAX_HEIGHT;
        request.timestamp = file.lastModified;
        request.cache = true;
        request.priority = 0;
        ImageLoaderClient.getInstance().load(request, resolve);
      }, reject);
    });
  }
}

/**
 * List of local volume types.
 *
 * In this context, "local" means that files in that volume are directly
 * accessible from the Chrome browser process by Linux VFS paths. In this
 * regard, media views are NOT local even though files in media views are
 * actually stored in the local disk.
 *
 * Due to access control of WebView, non-local files can not be previewed
 * with Quick View unless thumbnails are provided (which is the case with
 * Drive).
 *
 * @private @const {!Array<!VolumeManagerCommon.VolumeType>}
 */
QuickViewController.LOCAL_VOLUME_TYPES_ = [
  VolumeManagerCommon.VolumeType.ARCHIVE,
  VolumeManagerCommon.VolumeType.DOWNLOADS,
  VolumeManagerCommon.VolumeType.REMOVABLE,
  VolumeManagerCommon.VolumeType.ANDROID_FILES,
  VolumeManagerCommon.VolumeType.CROSTINI,
  VolumeManagerCommon.VolumeType.MEDIA_VIEW,
  VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
  VolumeManagerCommon.VolumeType.SMB,
];

/**
 * List of unsupported image subtypes excluded from being displayed in
 * QuickView. An "unsupported type" message is shown instead.
 * @private @const {!Array<string>}
 */
QuickViewController.UNSUPPORTED_IMAGE_SUBTYPES_ = [
  'TIFF',  // crbug.com/624109
];

/**
 * @typedef {{
 *   type: string,
 *   subtype: string,
 *   filePath: string,
 *   contentUrl: (string|undefined),
 *   videoPoster: (string|undefined),
 *   audioArtwork: (string|undefined),
 *   autoplay: (boolean|undefined),
 *   browsable: (boolean|undefined),
 * }}
 */
let QuickViewParams;

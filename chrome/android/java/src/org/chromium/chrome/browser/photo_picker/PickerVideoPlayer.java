// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.animation.Animator;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.VideoView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Encapsulates the video player functionality of the Photo Picker dialog.
 */
public class PickerVideoPlayer
        extends FrameLayout implements View.OnClickListener, SeekBar.OnSeekBarChangeListener {
    /**
     * A callback interface for notifying about video playback status.
     */
    public interface VideoPlaybackStatusCallback {
        // Called when the video starts playing.
        void onVideoPlaying();

        // Called when the video stops playing.
        void onVideoEnded();
    }

    // The callback to use for reporting playback progress in tests.
    private static VideoPlaybackStatusCallback sProgressCallback;

    // The resources to use.
    private Resources mResources;

    // The video preview view.
    private final VideoView mVideoView;

    // The MediaPlayer object used to control the VideoView.
    private MediaPlayer mMediaPlayer;

    // The container view for all the UI elements overlaid on top of the video.
    private final View mVideoOverlayContainer;

    // The container view for the UI video controls within the overlaid window.
    private final View mVideoControls;

    // The large Play button overlaid on top of the video.
    private final ImageView mLargePlayButton;

    // The Mute button for the video.
    private final ImageView mMuteButton;

    // Keeps track of whether audio track is enabled or not.
    private boolean mAudioOn = true;

    // The remaining video playback time.
    private final TextView mRemainingTime;

    // The SeekBar showing the video playback progress (allows user seeking).
    private final SeekBar mSeekBar;

    // A flag to control when the playback monitor schedules new tasks.
    private boolean mRunPlaybackMonitoringTask;

    /**
     * Constructor for inflating from XML.
     */
    public PickerVideoPlayer(Context context, AttributeSet attrs) {
        super(context, attrs);
        mResources = context.getResources();

        LayoutInflater.from(context).inflate(R.layout.video_player, this);

        mVideoView = findViewById(R.id.video_player);
        mVideoOverlayContainer = findViewById(R.id.video_overlay_container);
        mVideoControls = findViewById(R.id.video_controls);
        mLargePlayButton = findViewById(R.id.video_player_play_button);
        mMuteButton = findViewById(R.id.mute);
        mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
        mRemainingTime = findViewById(R.id.remaining_time);
        mSeekBar = findViewById(R.id.seek_bar);

        mVideoOverlayContainer.setOnClickListener(this);
        mLargePlayButton.setOnClickListener(this);
        mMuteButton.setOnClickListener(this);
        mSeekBar.setOnSeekBarChangeListener(this);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mVideoControls.getVisibility() != View.GONE) {
            // When configuration changes, the video overlay controls need to be synced to the new
            // video size. Post a task, so that size adjustments happen after layout of the video
            // controls has completed.
            ThreadUtils.postOnUiThread(() -> { syncOverlayControlsSize(); });
        }
    }

    /**
     * Start playback of a video in an overlay above the photo picker.
     * @param uri The uri of the video to start playing.
     */
    public void startVideoPlaybackAsync(Uri uri) {
        setVisibility(View.VISIBLE);

        mVideoView.setVisibility(View.VISIBLE);
        mVideoView.setVideoURI(uri);

        mVideoView.setOnPreparedListener((MediaPlayer mediaPlayer) -> {
            mMediaPlayer = mediaPlayer;
            startVideoPlayback();

            mMediaPlayer.setOnVideoSizeChangedListener(
                    (MediaPlayer player, int width, int height) -> { syncOverlayControlsSize(); });

            if (sProgressCallback != null) {
                mMediaPlayer.setOnInfoListener((MediaPlayer player, int what, int extra) -> {
                    if (what == MediaPlayer.MEDIA_INFO_VIDEO_RENDERING_START) {
                        sProgressCallback.onVideoPlaying();
                        return true;
                    }
                    return false;
                });
            }
        });

        mVideoView.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
            @Override
            public void onCompletion(MediaPlayer mediaPlayer) {
                // Once we reach the completion point, show the overlay controls (without fading
                // away) to indicate that playback has reached the end of the video (and didn't
                // break before reaching the end). This also allows the user to restart playback
                // from the start, by pressing Play.
                switchToPlayButton();
                updateProgress();
                showOverlayControls(/*animateAway=*/false);
                if (sProgressCallback != null) {
                    sProgressCallback.onVideoEnded();
                }
            }
        });
    }

    /**
     * Ends video playback (if a video is playing) and closes the video player. Aborts if the video
     * playback container is not showing.
     * @return true if a video container was showing, false otherwise.
     */
    public boolean closeVideoPlayer() {
        if (getVisibility() != View.VISIBLE) {
            return false;
        }

        setVisibility(View.GONE);
        stopVideoPlayback();
        mVideoView.setMediaController(null);
        mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
        return true;
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.video_overlay_container) {
            showOverlayControls(/*animateAway=*/true);
        } else if (id == R.id.video_player_play_button) {
            toggleVideoPlayback();
        } else if (id == R.id.mute) {
            toggleMute();
        }
    }

    // SeekBar.OnSeekBarChangeListener:

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            final boolean seekDuringPlay = mVideoView.isPlaying();
            mMediaPlayer.setOnSeekCompleteListener(mp -> {
                mMediaPlayer.setOnSeekCompleteListener(null);
                if (seekDuringPlay) {
                    startVideoPlayback();
                }
            });

            float percentage = progress / 100f;
            int seekTo = Math.round(percentage * mVideoView.getDuration());
            if (Build.VERSION.SDK_INT >= 26) {
                mMediaPlayer.seekTo(seekTo, MediaPlayer.SEEK_CLOSEST);
            } else {
                // On older versions, sync to nearest previous key frame.
                mVideoView.seekTo(seekTo);
            }
            updateProgress();
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        cancelFadeAwayAnimation();
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        fadeAwayVideoControls();
    }

    private void showOverlayControls(boolean animateAway) {
        cancelFadeAwayAnimation();

        if (animateAway && mVideoView.isPlaying()) {
            fadeAwayVideoControls();
            startPlaybackMonitor();
        }
    }

    private void fadeAwayVideoControls() {
        mVideoOverlayContainer.animate()
                .alpha(0.0f)
                .setStartDelay(3000)
                .setDuration(1000)
                .setListener(new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {}

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        enableClickableButtons(false);
                        stopPlaybackMonitor();
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {}

                    @Override
                    public void onAnimationRepeat(Animator animation) {}
                });
    }

    private void cancelFadeAwayAnimation() {
        // Canceling the animation will leave the alpha in the state it had reached while animating,
        // so we need to explicitly set the alpha to 1.0 to reset it.
        mVideoOverlayContainer.animate().cancel();
        mVideoOverlayContainer.setAlpha(1.0f);
        enableClickableButtons(true);
    }

    private void enableClickableButtons(boolean enable) {
        mLargePlayButton.setClickable(enable);
        mMuteButton.setClickable(enable);
    }

    private void updateProgress() {
        String current;
        String total;
        try {
            current = DecodeVideoTask.formatDuration(Long.valueOf(mVideoView.getCurrentPosition()));
            total = DecodeVideoTask.formatDuration(Long.valueOf(mVideoView.getDuration()));
        } catch (IllegalStateException exception) {
            // VideoView#getCurrentPosition throws this error if the dialog has been dismissed while
            // waiting to update the status.
            return;
        }

        String formattedProgress =
                mResources.getString(R.string.photo_picker_video_duration, current, total);
        mRemainingTime.setText(formattedProgress);
        mRemainingTime.setContentDescription(
                mResources.getString(R.string.accessibility_playback_time, current, total));
        int percentage = mVideoView.getDuration() == 0
                ? 0
                : mVideoView.getCurrentPosition() * 100 / mVideoView.getDuration();
        mSeekBar.setProgress(percentage);

        if (mVideoView.isPlaying() && mRunPlaybackMonitoringTask) {
            startPlaybackMonitor();
        }
    }

    private void startVideoPlayback() {
        mMediaPlayer.start();
        switchToPauseButton();
        showOverlayControls(/*animateAway=*/true);
    }

    private void stopVideoPlayback() {
        stopPlaybackMonitor();

        mMediaPlayer.pause();
        switchToPlayButton();
        showOverlayControls(/*animateAway=*/false);
    }

    private void toggleVideoPlayback() {
        if (mVideoView.isPlaying()) {
            stopVideoPlayback();
        } else {
            startVideoPlayback();
        }
    }

    private void switchToPlayButton() {
        mLargePlayButton.setImageResource(R.drawable.ic_play_circle_filled_white_24dp);
        mLargePlayButton.setContentDescription(
                mResources.getString(R.string.accessibility_play_video));
    }

    private void switchToPauseButton() {
        mLargePlayButton.setImageResource(R.drawable.ic_pause_circle_outline_white_24dp);
        mLargePlayButton.setContentDescription(
                mResources.getString(R.string.accessibility_pause_video));
    }

    private void syncOverlayControlsSize() {
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                mVideoView.getMeasuredWidth(), mVideoView.getMeasuredHeight());
        mVideoControls.setLayoutParams(params);
    }

    private void toggleMute() {
        mAudioOn = !mAudioOn;
        if (mAudioOn) {
            mMediaPlayer.setVolume(1f, 1f);
            mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
            mMuteButton.setContentDescription(
                    mResources.getString(R.string.accessibility_mute_video));
        } else {
            mMediaPlayer.setVolume(0f, 0f);
            mMuteButton.setImageResource(R.drawable.ic_volume_off_white_24dp);
            mMuteButton.setContentDescription(
                    mResources.getString(R.string.accessibility_unmute_video));
        }
    }

    private void startPlaybackMonitor() {
        mRunPlaybackMonitoringTask = true;
        startPlaybackMonitorTask();
    }

    private void startPlaybackMonitorTask() {
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> updateProgress(), 250);
    }

    private void stopPlaybackMonitor() {
        mRunPlaybackMonitoringTask = false;
    }

    /** Sets the video playback progress callback. For testing use only. */
    @VisibleForTesting
    public static void setProgressCallback(VideoPlaybackStatusCallback callback) {
        sProgressCallback = callback;
    }
}

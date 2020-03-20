/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for MediaStream, MediaTrack and MediaSource.
// These interfaces are used for implementing MediaStream and MediaTrack as
// defined in http://dev.w3.org/2011/webrtc/editor/webrtc.html#stream-api. These
// interfaces must be used only with PeerConnection.

#ifndef API_MEDIA_STREAM_INTERFACE_H_
#define API_MEDIA_STREAM_INTERFACE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_options.h"
#include "api/scoped_refptr.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/system/rtc_export.h"

#if defined(WEBRTC_IOS)

#import <AudioToolbox/AudioToolbox.h>

#import <CoreAudio/CoreAudioTypes.h>
#import <AudioToolbox/AudioQueue.h>
#import <AudioUnit/AudioUnit.h>

#endif

#include "rtc_base/buffer.h"

   
namespace webrtc {

#if defined(WEBRTC_IOS)
typedef struct {
//    AudioUnitRenderActionFlags* flags;
//    const AudioTimeStamp* time_stamp;
//    uint32_t bus_number;
//    uint32_t num_frames;
//    AudioBufferList* io_data;
    
//    rtc::BufferT<int16_t> buffer;
//    int record_delay;

    const int16_t *buffer;
    size_t buffer_size;
    int record_delay;
} AudioSample;

#else
typedef struct { } AudioSample;
#warning "//TODO: Implement audio sampling for other platforms"
#endif

// Generic observer interface.
class ObserverInterface {
 public:
  virtual void OnChanged() = 0;

 protected:
  virtual ~ObserverInterface() {}
};

class NotifierInterface {
 public:
  virtual void RegisterObserver(ObserverInterface* observer) = 0;
  virtual void UnregisterObserver(ObserverInterface* observer) = 0;

  virtual ~NotifierInterface() {}
};

// Base class for sources. A MediaStreamTrack has an underlying source that
// provides media. A source can be shared by multiple tracks.
class RTC_EXPORT MediaSourceInterface : public rtc::RefCountInterface,
                                        public NotifierInterface {
 public:
  enum SourceState { kInitializing, kLive, kEnded, kMuted };

  virtual SourceState state() const = 0;

  virtual bool remote() const = 0;

 protected:
  ~MediaSourceInterface() override = default;
};

// C++ version of MediaStreamTrack.
// See: https://www.w3.org/TR/mediacapture-streams/#mediastreamtrack
class RTC_EXPORT MediaStreamTrackInterface : public rtc::RefCountInterface,
                                             public NotifierInterface {
 public:
  enum TrackState {
    kLive,
    kEnded,
  };

  static const char* const kAudioKind;
  static const char* const kVideoKind;

  // The kind() method must return kAudioKind only if the object is a
  // subclass of AudioTrackInterface, and kVideoKind only if the
  // object is a subclass of VideoTrackInterface. It is typically used
  // to protect a static_cast<> to the corresponding subclass.
  virtual std::string kind() const = 0;

  // Track identifier.
  virtual std::string id() const = 0;

  // A disabled track will produce silence (if audio) or black frames (if
  // video). Can be disabled and re-enabled.
  virtual bool enabled() const = 0;
  virtual bool set_enabled(bool enable) = 0;

  // Live or ended. A track will never be live again after becoming ended.
  virtual TrackState state() const = 0;

 protected:
  ~MediaStreamTrackInterface() override = default;
};

// VideoTrackSourceInterface is a reference counted source used for
// VideoTracks. The same source can be used by multiple VideoTracks.
// VideoTrackSourceInterface is designed to be invoked on the signaling thread
// except for rtc::VideoSourceInterface<VideoFrame> methods that will be invoked
// on the worker thread via a VideoTrack. A custom implementation of a source
// can inherit AdaptedVideoTrackSource instead of directly implementing this
// interface.
class VideoTrackSourceInterface : public MediaSourceInterface,
                                  public rtc::VideoSourceInterface<VideoFrame> {
 public:
  struct Stats {
    // Original size of captured frame, before video adaptation.
    int input_width;
    int input_height;
  };

  // Indicates that parameters suitable for screencasts should be automatically
  // applied to RtpSenders.
  // TODO(perkj): Remove these once all known applications have moved to
  // explicitly setting suitable parameters for screencasts and don't need this
  // implicit behavior.
  virtual bool is_screencast() const = 0;

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  // TODO(perkj): Remove this once denoising is done by the source, and not by
  // the encoder.
  virtual absl::optional<bool> needs_denoising() const = 0;

  // Returns false if no stats are available, e.g, for a remote source, or a
  // source which has not seen its first frame yet.
  //
  // Implementation should avoid blocking.
  virtual bool GetStats(Stats* stats) = 0;

  // Returns true if encoded output can be enabled in the source.
  // TODO(bugs.webrtc.org/11114): make pure virtual once downstream project
  // adapts.
  virtual bool SupportsEncodedOutput() const { return false; }

  // Reliably cause a key frame to be generated in encoded output.
  // TODO(bugs.webrtc.org/11115): find optimal naming.
  // TODO(bugs.webrtc.org/11114): make pure virtual once downstream project
  // adapts.
  virtual void GenerateKeyFrame() {}

  // Add an encoded video sink to the source and additionally cause
  // a key frame to be generated from the source. The sink will be
  // invoked from a decoder queue.
  // TODO(bugs.webrtc.org/11114): make pure virtual once downstream project
  // adapts.
  virtual void AddEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) {}

  // Removes an encoded video sink from the source.
  // TODO(bugs.webrtc.org/11114): make pure virtual once downstream project
  // adapts.
  virtual void RemoveEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) {}

 protected:
  ~VideoTrackSourceInterface() override = default;
};

// VideoTrackInterface is designed to be invoked on the signaling thread except
// for rtc::VideoSourceInterface<VideoFrame> methods that must be invoked
// on the worker thread.
// PeerConnectionFactory::CreateVideoTrack can be used for creating a VideoTrack
// that ensures thread safety and that all methods are called on the right
// thread.
class RTC_EXPORT VideoTrackInterface
    : public MediaStreamTrackInterface,
      public rtc::VideoSourceInterface<VideoFrame> {
 public:
  // Video track content hint, used to override the source is_screencast
  // property.
  // See https://crbug.com/653531 and https://w3c.github.io/mst-content-hint.
  enum class ContentHint { kNone, kFluid, kDetailed, kText };

  // Register a video sink for this track. Used to connect the track to the
  // underlying video engine.
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {}
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {}

  virtual VideoTrackSourceInterface* GetSource() const = 0;

  virtual ContentHint content_hint() const;
  virtual void set_content_hint(ContentHint hint) {}

 protected:
  ~VideoTrackInterface() override = default;
};

// Interface for receiving audio data from a AudioTrack.
class AudioTrackSinkInterface {
 public:
  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames) {
    RTC_NOTREACHED() << "This method must be overridden, or not used.";
  }

  // In this method, |absolute_capture_timestamp_ms|, when available, is
  // supposed to deliver the timestamp when this audio frame was originally
  // captured. This timestamp MUST be based on the same clock as
  // rtc::TimeMillis().
  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames,
                      absl::optional<int64_t> absolute_capture_timestamp_ms) {
    // TODO(bugs.webrtc.org/10739): Deprecate the old OnData and make this one
    // pure virtual.
    return OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                  number_of_frames);
  }

 protected:
  virtual ~AudioTrackSinkInterface() {}
};

// AudioSourceInterface is a reference counted source used for AudioTracks.
// The same source can be used by multiple AudioTracks.
class RTC_EXPORT AudioSourceInterface : public MediaSourceInterface {
 public:
  class AudioObserver {
   public:
    virtual void OnSetVolume(double volume) = 0;

   protected:
    virtual ~AudioObserver() {}
  };

  // TODO(deadbeef): Makes all the interfaces pure virtual after they're
  // implemented in chromium.

  // Sets the volume of the source. |volume| is in  the range of [0, 10].
  // TODO(tommi): This method should be on the track and ideally volume should
  // be applied in the track in a way that does not affect clones of the track.
  virtual void SetVolume(double volume) {}

  // Registers/unregisters observers to the audio source.
  virtual void RegisterAudioObserver(AudioObserver* observer) {}
  virtual void UnregisterAudioObserver(AudioObserver* observer) {}

  // TODO(tommi): Make pure virtual.
  virtual void AddSink(AudioTrackSinkInterface* sink) {}
  virtual void RemoveSink(AudioTrackSinkInterface* sink) {}
  virtual bool putAudioSample(const AudioSample &sample) {
    printf("[AudioSourceInterface] putAudioSample not impl.");
    return false;
  }


  // Returns options for the AudioSource.
  // (for some of the settings this approach is broken, e.g. setting
  // audio network adaptation on the source is the wrong layer of abstraction).
  virtual const cricket::AudioOptions options() const;
};

// Interface of the audio processor used by the audio track to collect
// statistics.
class AudioProcessorInterface : public rtc::RefCountInterface {
 public:
  struct AudioProcessorStatistics {
    bool typing_noise_detected = false;
    AudioProcessingStats apm_statistics;
  };

  // Get audio processor statistics. The |has_remote_tracks| argument should be
  // set if there are active remote tracks (this would usually be true during
  // a call). If there are no remote tracks some of the stats will not be set by
  // the AudioProcessor, because they only make sense if there is at least one
  // remote track.
  virtual AudioProcessorStatistics GetStats(bool has_remote_tracks) = 0;

 protected:
  ~AudioProcessorInterface() override = default;
};

class RTC_EXPORT AudioTrackInterface : public MediaStreamTrackInterface {
 public:
  // TODO(deadbeef): Figure out if the following interface should be const or
  // not.
  virtual AudioSourceInterface* GetSource() const = 0;

  // Add/Remove a sink that will receive the audio data from the track.
  virtual void AddSink(AudioTrackSinkInterface* sink) = 0;
  virtual void RemoveSink(AudioTrackSinkInterface* sink) = 0;

  // Get the signal level from the audio track.
  // Return true on success, otherwise false.
  // TODO(deadbeef): Change the interface to int GetSignalLevel() and pure
  // virtual after it's implemented in chromium.
  virtual bool GetSignalLevel(int* level);

  // Get the audio processor used by the audio track. Return null if the track
  // does not have any processor.
  // TODO(deadbeef): Make the interface pure virtual.
  virtual rtc::scoped_refptr<AudioProcessorInterface> GetAudioProcessor();

 protected:
  ~AudioTrackInterface() override = default;
};

typedef std::vector<rtc::scoped_refptr<AudioTrackInterface> > AudioTrackVector;
typedef std::vector<rtc::scoped_refptr<VideoTrackInterface> > VideoTrackVector;

// C++ version of https://www.w3.org/TR/mediacapture-streams/#mediastream.
//
// A major difference is that remote audio/video tracks (received by a
// PeerConnection/RtpReceiver) are not synchronized simply by adding them to
// the same stream; a session description with the correct "a=msid" attributes
// must be pushed down.
//
// Thus, this interface acts as simply a container for tracks.
class MediaStreamInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  virtual std::string id() const = 0;

  virtual AudioTrackVector GetAudioTracks() = 0;
  virtual VideoTrackVector GetVideoTracks() = 0;
  virtual rtc::scoped_refptr<AudioTrackInterface> FindAudioTrack(
      const std::string& track_id) = 0;
  virtual rtc::scoped_refptr<VideoTrackInterface> FindVideoTrack(
      const std::string& track_id) = 0;

  virtual bool AddTrack(AudioTrackInterface* track) = 0;
  virtual bool AddTrack(VideoTrackInterface* track) = 0;
  virtual bool RemoveTrack(AudioTrackInterface* track) = 0;
  virtual bool RemoveTrack(VideoTrackInterface* track) = 0;

 protected:
  ~MediaStreamInterface() override = default;
};

}  // namespace webrtc

#endif  // API_MEDIA_STREAM_INTERFACE_H_

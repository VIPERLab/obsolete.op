/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_SYNC_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_SYNC_BUFFER_H_

#include "webrtc/modules/audio_coding/neteq4/audio_multi_vector.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class SyncBuffer : public AudioMultiVector<int16_t> {
 public:
  SyncBuffer(std::size_t channels, std::size_t length)
      : AudioMultiVector<int16_t>(channels, length),
        next_index_(length),
        end_timestamp_(0),
        dtmf_index_(0) {}

  virtual ~SyncBuffer() {}

  // Returns the number of samples yet to play out form the buffer.
  std::size_t FutureLength() const;

  // Adds the contents of |append_this| to the back of the SyncBuffer. Removes
  // the same number of samples from the beginning of the SyncBuffer, to
  // maintain a constant buffer size. The |next_index_| is updated to reflect
  // the move of the beginning of "future" data.
  void PushBack(const AudioMultiVector<int16_t>& append_this);

  // Adds |length| zeros to the beginning of each channel. Removes
  // the same number of samples from the end of the SyncBuffer, to
  // maintain a constant buffer size. The |next_index_| is updated to reflect
  // the move of the beginning of "future" data.
  // Note that this operation may delete future samples that are waiting to
  // be played.
  void PushFrontZeros(std::size_t length);

  // Inserts |length| zeros into each channel at index |position|. The size of
  // the SyncBuffer is kept constant, which means that the last |length|
  // elements in each channel will be purged.
  virtual void InsertZerosAtIndex(std::size_t length, std::size_t position);

  // Overwrites each channel in this SyncBuffer with values taken from
  // |insert_this|. The values are taken from the beginning of |insert_this| and
  // are inserted starting at |position|. |length| values are written into each
  // channel. The size of the SyncBuffer is kept constant. That is, if |length|
  // and |position| are selected such that the new data would extend beyond the
  // end of the current SyncBuffer, the buffer is not extended.
  // The |next_index_| is not updated.
  virtual void ReplaceAtIndex(const AudioMultiVector<int16_t>& insert_this,
		  std::size_t length,
		  std::size_t position);

  // Same as the above method, but where all of |insert_this| is written (with
  // the same constraints as above, that the SyncBuffer is not extended).
  virtual void ReplaceAtIndex(const AudioMultiVector<int16_t>& insert_this,
		  std::size_t position);

  // Reads |requested_len| samples from each channel and writes them interleaved
  // into |output|. The |next_index_| is updated to point to the sample to read
  // next time.
  std::size_t GetNextAudioInterleaved(std::size_t requested_len, int16_t* output);

  // Adds |increment| to |end_timestamp_|.
  void IncreaseEndTimestamp(uint32_t increment);

  // Flushes the buffer. The buffer will contain only zeros after the flush, and
  // |next_index_| will point to the end, like when the buffer was first
  // created.
  void Flush();

  const AudioVector<int16_t>& Channel(std::size_t n) { return *channels_[n]; }

  // Accessors and mutators.
  std::size_t next_index() const { return next_index_; }
  void set_next_index(std::size_t value);
  uint32_t end_timestamp() const { return end_timestamp_; }
  void set_end_timestamp(uint32_t value) { end_timestamp_ = value; }
  std::size_t dtmf_index() const { return dtmf_index_; }
  void set_dtmf_index(std::size_t value);

 private:
  std::size_t next_index_;
  uint32_t end_timestamp_;  // The timestamp of the last sample in the buffer.
  std::size_t dtmf_index_;  // Index to the first non-DTMF sample in the buffer.

  DISALLOW_COPY_AND_ASSIGN(SyncBuffer);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_SYNC_BUFFER_H_

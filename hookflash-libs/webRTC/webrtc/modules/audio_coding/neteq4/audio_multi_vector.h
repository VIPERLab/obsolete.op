/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_AUDIO_MULTI_VECTOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_AUDIO_MULTI_VECTOR_H_

#include <cstring>  // Access to size_t.
#include <vector>

#include "webrtc/modules/audio_coding/neteq4/audio_vector.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"

namespace webrtc {

template <typename T>
class AudioMultiVector {
 public:
  // Creates an empty AudioMultiVector with |N| audio channels. |N| must be
  // larger than 0.
  explicit AudioMultiVector(std::size_t N);

  // Creates an AudioMultiVector with |N| audio channels, each channel having
  // an initial size. |N| must be larger than 0.
  AudioMultiVector(std::size_t N, std::size_t initial_size);

  virtual ~AudioMultiVector();

  // Deletes all values and make the vector empty.
  virtual void Clear();

  // Clears the vector and inserts |length| zeros into each channel.
  virtual void Zeros(std::size_t length);

  // Copies all values from this vector to |copy_to|. Any contents in |copy_to|
  // are deleted. After the operation is done, |copy_to| will be an exact
  // replica of this object. The source and the destination must have the same
  // number of channels.
  virtual void CopyFrom(AudioMultiVector<T>* copy_to) const;

  // Appends the contents of array |append_this| to the end of this
  // object. The array is assumed to be channel-interleaved. |length| must be
  // an even multiple of this object's number of channels.
  // The length of this object is increased with the |length| divided by the
  // number of channels.
  virtual void PushBackInterleaved(const T* append_this, std::size_t length);

  // Appends the contents of AudioMultiVector |append_this| to this object. The
  // length of this object is increased with the length of |append_this|.
  virtual void PushBack(const AudioMultiVector<T>& append_this);

  // Appends the contents of AudioMultiVector |append_this| to this object,
  // taken from |index| up until the end of |append_this|. The length of this
  // object is increased.
  virtual void PushBackFromIndex(const AudioMultiVector<T>& append_this,
		  std::size_t index);

  // Removes |length| elements from the beginning of this object, from each
  // channel.
  virtual void PopFront(std::size_t length);

  // Removes |length| elements from the end of this object, from each
  // channel.
  virtual void PopBack(std::size_t length);

  // Reads |length| samples from each channel and writes them interleaved to
  // |destination|. The total number of elements written to |destination| is
  // returned, i.e., |length| * number of channels. If the AudioMultiVector
  // contains less than |length| samples per channel, this is reflected in the
  // return value.
  virtual std::size_t ReadInterleaved(std::size_t length, T* destination) const;

  // Like ReadInterleaved() above, but reads from |start_index| instead of from
  // the beginning.
  virtual std::size_t ReadInterleavedFromIndex(std::size_t start_index,
		  std::size_t length,
                                          T* destination) const;

  // Like ReadInterleaved() above, but reads from the end instead of from
  // the beginning.
  virtual std::size_t ReadInterleavedFromEnd(std::size_t length,
                                        T* destination) const;

  // Overwrites each channel in this AudioMultiVector with values taken from
  // |insert_this|. The values are taken from the beginning of |insert_this| and
  // are inserted starting at |position|. |length| values are written into each
  // channel. If |length| and |position| are selected such that the new data
  // extends beyond the end of the current AudioVector, the vector is extended
  // to accommodate the new data. |length| is limited to the length of
  // |insert_this|.
  virtual void OverwriteAt(const AudioMultiVector<T>& insert_this,
		  std::size_t length,
		  std::size_t position);

  // Appends |append_this| to the end of the current vector. Lets the two
  // vectors overlap by |fade_length| samples (per channel), and cross-fade
  // linearly in this region.
  virtual void CrossFade(const AudioMultiVector<T>& append_this,
		  std::size_t fade_length);

  // Returns the number of channels.
  virtual std::size_t Channels() const { return channels_.size(); }

  // Returns the number of elements per channel in this AudioMultiVector.
  virtual std::size_t Size() const;

  // Verify that each channel can hold at least |required_size| elements. If
  // not, extend accordingly.
  virtual void AssertSize(std::size_t required_size);

  virtual bool Empty() const;

  // Accesses and modifies a channel (i.e., an AudioVector object) of this
  // AudioMultiVector.
  const AudioVector<T>& operator[](std::size_t index) const;
  AudioVector<T>& operator[](std::size_t index);

 protected:
  std::vector<AudioVector<T>*> channels_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioMultiVector);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_AUDIO_MULTI_VECTOR_H_

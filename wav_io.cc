/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Functions to write audio in WAV format.

#include <math.h>
#include <string.h>
#include <algorithm>

#include "wav_io.h"
#include "status.h"

using std::string;

namespace {

  static const uint8_t kuint8max = ((uint8_t)0xFF);
  static const uint16_t kuint16max = ((uint16_t)0xFFFF);
  static const uint32_t kuint32max = ((uint32_t)0xFFFFFFFF);
  static const uint64_t kuint64max = ((uint64_t)0xFFFFFFFFFFFFFFFFull);
  static const int8_t kint8min = ((int8_t)~0x7F);
  static const int8_t kint8max = ((int8_t)0x7F);
  static const int16_t kint16min = ((int16_t)~0x7FFF);
  static const int16_t kint16max = ((int16_t)0x7FFF);
  static const int32_t kint32min = ((int32_t)~0x7FFFFFFF);
  static const int32_t kint32max = ((int32_t)0x7FFFFFFF);
  static const int64_t kint64min = ((int64_t)~0x7FFFFFFFFFFFFFFFll);
  static const int64_t kint64max = ((int64_t)0x7FFFFFFFFFFFFFFFll);

struct __attribute((packed)) RiffChunk {
  char chunk_id[4];
  char chunk_data_size[4];
  char riff_type[4];
};
static_assert(sizeof(RiffChunk) == 12, "__attribute((packed)) does not work.");

struct __attribute((packed)) FormatChunk {
  char chunk_id[4];
  char chunk_data_size[4];
  char compression_code[2];
  char channel_numbers[2];
  char sample_rate[4];
  char bytes_per_second[4];
  char bytes_per_frame[2];
  char bits_per_sample[2];
};
static_assert(sizeof(FormatChunk) == 24, "__attribute((packed)) does not work.");

struct __attribute((packed)) DataChunk {
  char chunk_id[4];
  char chunk_data_size[4];
};
static_assert(sizeof(DataChunk) == 8, "__attribute((packed)) does not work.");

struct __attribute((packed)) WavHeader {
  RiffChunk riff_chunk;
  FormatChunk format_chunk;
  DataChunk data_chunk;
};
static_assert(sizeof(WavHeader) ==
                  sizeof(RiffChunk) + sizeof(FormatChunk) + sizeof(DataChunk),
              "__attribute((packed)) does not work.");

constexpr char kRiffChunkId[] = "RIFF";
constexpr char kRiffType[] = "WAVE";
constexpr char kFormatChunkId[] = "fmt ";
constexpr char kDataChunkId[] = "data";

inline int16_t FloatToInt16Sample(float data) {
  constexpr float kMultiplier = 1.0f * (1 << 15);
  return std::min<float>(std::max<float>(roundf(data * kMultiplier), kint16min),
                         kint16max);
}

inline float Int16SampleToFloat(int16_t data) {
  constexpr float kMultiplier = 1.0f / (1 << 15);
  return data * kMultiplier;
}

Status ExpectText(const uint8_t* data, size_t data_length, const string& expected_text,
                  int* offset) {
  const int new_offset = *offset + expected_text.size();
  if (new_offset > data_length) {
    return errors::InvalidArgument("Data too short when trying to read ",
                                   expected_text);
  }
  const string found_text(data + *offset, data + new_offset);
  if (found_text != expected_text) {
    return errors::InvalidArgument("Header mismatch: Expected ", expected_text,
                                   " but found ", found_text);
  }
  *offset = new_offset;
  return Status::OK();
}

template <class T>
Status ReadValue(const uint8_t* data, size_t data_length, T* value, int* offset) {
  const int new_offset = *offset + sizeof(T);
  if (new_offset > data_length) {
    return errors::InvalidArgument("Data too short when trying to read value");
  }
  memcpy(value, data + *offset, sizeof(T));
  *offset = new_offset;
  return Status::OK();
}

Status ReadString(const uint8_t* data, size_t data_length, int expected_length, string* value,
                  int* offset) {
  const int new_offset = *offset + expected_length;
  if (new_offset > data_length) {
    return errors::InvalidArgument("Data too short when trying to read string");
  }
  *value = string(data + *offset, data + new_offset);
  *offset = new_offset;
  return Status::OK();
}


  template <class Dest, class Source>
  inline Dest bit_cast(const Source& source) {
    static_assert(sizeof(Dest) == sizeof(Source), "Sizes do not match");

    Dest dest;
    memcpy(&dest, &source, sizeof(dest));
    return dest;
  }

  void EncodeFixed16(char* buf, uint16_t value) {
      memcpy(buf, &value, sizeof(value));
  }

  void EncodeFixed32(char* buf, uint32_t value) {
      memcpy(buf, &value, sizeof(value));
  }

}  // namespace

Status EncodeAudioAsS16LEWav(const float* audio, size_t sample_rate,
                             size_t num_channels, size_t num_frames,
                             string* wav_string) {
  constexpr size_t kFormatChunkSize = 16;
  constexpr size_t kCompressionCodePcm = 1;
  constexpr size_t kBitsPerSample = 16;
  constexpr size_t kBytesPerSample = kBitsPerSample / 8;
  constexpr size_t kHeaderSize = sizeof(WavHeader);

  if (audio == nullptr) {
    return errors::InvalidArgument("audio is null");
  }
  if (wav_string == nullptr) {
    return errors::InvalidArgument("wav_string is null");
  }
  if (sample_rate == 0 || sample_rate > kuint32max) {
    return errors::InvalidArgument("sample_rate must be in (0, 2^32), got: ",
                                   sample_rate);
  }
  if (num_channels == 0 || num_channels > kuint16max) {
    return errors::InvalidArgument("num_channels must be in (0, 2^16), got: ",
                                   num_channels);
  }
  if (num_frames == 0) {
    return errors::InvalidArgument("num_frames must be positive.");
  }

  const size_t bytes_per_second = sample_rate * kBytesPerSample;
  const size_t num_samples = num_frames * num_channels;
  const size_t data_size = num_samples * kBytesPerSample;
  const size_t file_size = kHeaderSize + num_samples * kBytesPerSample;
  const size_t bytes_per_frame = kBytesPerSample * num_channels;

  // WAV represents the length of the file as a uint32 so file_size cannot
  // exceed kuint32max.
  if (file_size > kuint32max) {
    return errors::InvalidArgument(
        "Provided channels and frames cannot be encoded as a WAV.");
  }

  wav_string->resize(file_size);
  char* data = &wav_string->at(0);
  WavHeader* header = bit_cast<WavHeader*>(data);

  // Fill RIFF chunk.
  auto* riff_chunk = &header->riff_chunk;
  memcpy(riff_chunk->chunk_id, kRiffChunkId, 4);
  EncodeFixed32(riff_chunk->chunk_data_size, file_size - 8);
  memcpy(riff_chunk->riff_type, kRiffType, 4);

  // Fill format chunk.
  auto* format_chunk = &header->format_chunk;
  memcpy(format_chunk->chunk_id, kFormatChunkId, 4);
  EncodeFixed32(format_chunk->chunk_data_size, kFormatChunkSize);
  EncodeFixed16(format_chunk->compression_code, kCompressionCodePcm);
  EncodeFixed16(format_chunk->channel_numbers, num_channels);
  EncodeFixed32(format_chunk->sample_rate, sample_rate);
  EncodeFixed32(format_chunk->bytes_per_second, bytes_per_second);
  EncodeFixed16(format_chunk->bytes_per_frame, bytes_per_frame);
  EncodeFixed16(format_chunk->bits_per_sample, kBitsPerSample);

  // Fill data chunk.
  auto* data_chunk = &header->data_chunk;
  memcpy(data_chunk->chunk_id, kDataChunkId, 4);
  EncodeFixed32(data_chunk->chunk_data_size, data_size);

  // Write the audio.
  data += kHeaderSize;
  for (size_t i = 0; i < num_samples; ++i) {
    int16_t sample = FloatToInt16Sample(audio[i]);
    EncodeFixed16(&data[i * kBytesPerSample],
                        static_cast<uint16_t>(sample));
  }
  return Status::OK();
}

Status DecodeLin16WaveAsFloatVector(const uint8_t* wav_data,
                                    size_t wav_length,
                                    std::vector<float>* float_values,
                                    uint32_t* sample_count, uint16_t* channel_count,
                                    uint32_t* sample_rate) {
  int offset = 0;
  TF_RETURN_IF_ERROR(ExpectText(wav_data, wav_length, kRiffChunkId, &offset));
  uint32_t total_file_size;
  TF_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_data, wav_length, &total_file_size, &offset));
  TF_RETURN_IF_ERROR(ExpectText(wav_data, wav_length, kRiffType, &offset));
  TF_RETURN_IF_ERROR(ExpectText(wav_data, wav_length, kFormatChunkId, &offset));
  uint32_t format_chunk_size;
  TF_RETURN_IF_ERROR(
      ReadValue<uint32_t>(wav_data, wav_length, &format_chunk_size, &offset));
  if ((format_chunk_size != 16) && (format_chunk_size != 18)) {
    return errors::InvalidArgument(
        "Bad file size for WAV: Expected 16 or 18, but got", format_chunk_size);
  }
  uint16_t audio_format;
  TF_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_data, wav_length, &audio_format, &offset));
  if (audio_format != 1) {
    return errors::InvalidArgument(
        "Bad audio format for WAV: Expected 1 (PCM), but got", audio_format);
  }
  TF_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_data, wav_length, channel_count, &offset));
  TF_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_data, wav_length, sample_rate, &offset));
  uint32_t bytes_per_second;
  TF_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_data, wav_length, &bytes_per_second, &offset));
  uint16_t bytes_per_sample;
  TF_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_data, wav_length, &bytes_per_sample, &offset));
  // Confusingly, bits per sample is defined as holding the number of bits for
  // one channel, unlike the definition of sample used elsewhere in the WAV
  // spec. For example, bytes per sample is the memory needed for all channels
  // for one point in time.
  uint16_t bits_per_sample;
  TF_RETURN_IF_ERROR(ReadValue<uint16_t>(wav_data, wav_length, &bits_per_sample, &offset));
  if (bits_per_sample != 16) {
    return errors::InvalidArgument(
        "Can only read 16-bit WAV files, but received ", bits_per_sample);
  }
  const uint32_t expected_bytes_per_sample =
      ((bits_per_sample * *channel_count) + 7) / 8;
  if (bytes_per_sample != expected_bytes_per_sample) {
    return errors::InvalidArgument(
        "Bad bytes per sample in WAV header: Expected ",
        expected_bytes_per_sample, " but got ", bytes_per_sample);
  }
  const uint32_t expected_bytes_per_second =
      (bytes_per_sample * (*sample_rate));
  if (bytes_per_second != expected_bytes_per_second) {
    return errors::InvalidArgument(
        "Bad bytes per second in WAV header: Expected ",
        expected_bytes_per_second, " but got ", bytes_per_second,
        " (sample_rate=", *sample_rate, ", bytes_per_sample=", bytes_per_sample,
        ")");
  }
  if (format_chunk_size == 18) {
    // Skip over this unused section.
    offset += 2;
  }

  bool was_data_found = false;
  while (offset < wav_length) {
    string chunk_id;
    TF_RETURN_IF_ERROR(ReadString(wav_data, wav_length, 4, &chunk_id, &offset));
    uint32_t chunk_size;
    TF_RETURN_IF_ERROR(ReadValue<uint32_t>(wav_data, wav_length, &chunk_size, &offset));
    if (chunk_id == kDataChunkId) {
      if (was_data_found) {
        return errors::InvalidArgument("More than one data chunk found in WAV");
      }
      was_data_found = true;
      *sample_count = chunk_size / bytes_per_sample;
      const uint32_t data_count = *sample_count * *channel_count;
      float_values->resize(data_count);
      for (int i = 0; i < data_count; ++i) {
        int16_t single_channel_value = 0;
        TF_RETURN_IF_ERROR(
            ReadValue<int16_t>(wav_data, wav_length, &single_channel_value, &offset));
        (*float_values)[i] = Int16SampleToFloat(single_channel_value);
      }
    } else {
      offset += chunk_size;
    }
  }
  if (!was_data_found) {
    return errors::InvalidArgument("No data chunk found in WAV");
  }
  return Status::OK();
}

// Implements the functionality defined in packer.h

#include "packer.h"

#include <string>
#include "logging.h"

namespace ncode {

std::string PackedUintSeq::MemString() const {
  std::string return_string;

  return_string += "num_elements: " + std::to_string(len_) + ", size: " +
                   std::to_string(SizeBytes()) + "bytes, array_len: " +
                   std::to_string(data_.size());
  return return_string;
}

void PackedUintSeq::Append(uint64_t value, size_t* bytes) {
  CHECK(value >= last_append_) << "Sequence non-incrementing last is " +
                                      std::to_string(last_append_) +
                                      " new is " + std::to_string(value);
  const uint64_t diff = value - last_append_;

  if (diff < kOneByteLimit) {
    data_.push_back(diff);

    *bytes += sizeof(uint8_t);
  } else if (diff < kTwoByteLimit) {
    data_.push_back((diff >> 8) | kTwoBytesPacked);
    data_.push_back(diff);

    *bytes += 2 * sizeof(uint8_t);
  } else if (diff < kThreeByteLimit) {
    data_.push_back(((diff >> 16) | kThreeBytesPacked));
    data_.push_back(diff >> 8);
    data_.push_back(diff >> 0);

    *bytes += 3 * sizeof(uint8_t);
  } else if (diff < kFourByteLimit) {
    data_.push_back((diff >> 24) | kFourBytesPacked);
    data_.push_back(diff >> 16);
    data_.push_back(diff >> 8);
    data_.push_back(diff);

    *bytes += 4 * sizeof(uint8_t);
  } else if (diff < kFiveByteLimit) {
    data_.push_back((diff >> 32) | kFiveBytesPacked);
    data_.push_back(diff >> 24);
    data_.push_back(diff >> 16);
    data_.push_back(diff >> 8);
    data_.push_back(diff);

    *bytes += 5 * sizeof(uint8_t);
  } else if (diff < kSixByteLimit) {
    data_.push_back((diff >> 40) | kSixBytesPacked);
    data_.push_back(diff >> 32);
    data_.push_back(diff >> 24);
    data_.push_back(diff >> 16);
    data_.push_back(diff >> 8);
    data_.push_back(diff);

    *bytes += 6 * sizeof(uint8_t);
  } else if (diff < kSevenByteLimit) {
    data_.push_back((diff >> 48) | kSevenBytesPacked);
    data_.push_back(diff >> 40);
    data_.push_back(diff >> 32);
    data_.push_back(diff >> 24);
    data_.push_back(diff >> 16);
    data_.push_back(diff >> 8);
    data_.push_back(diff);

    *bytes += 7 * sizeof(uint8_t);
  } else if (diff < kEightByteLimit) {
    data_.push_back((diff >> 56) | kEightBytesPacked);
    data_.push_back(diff >> 48);
    data_.push_back(diff >> 40);
    data_.push_back(diff >> 32);
    data_.push_back(diff >> 24);
    data_.push_back(diff >> 16);
    data_.push_back(diff >> 8);
    data_.push_back(diff);

    *bytes += 8 * sizeof(uint8_t);
  } else {
    CHECK(false) << "Difference too large " + std::to_string(diff);
  }

  len_++;
  last_append_ = value;
}

size_t PackedUintSeq::DeflateSingleInteger(const size_t offset,
                                           uint64_t* value) const {
  const uint8_t c = data_[offset];

  // the 3 most significant bits, kEightBytesPacked is the inverse of kMask
  const u_char sign_bits = (c & kEightBytesPacked);
  switch (sign_bits) {
    case kOneBytePacked: {
      *value = (static_cast<uint64_t>(c) & kMask);
      return 1;
    }
    case kTwoBytesPacked: {
      *value = ((static_cast<uint64_t>(c) & kMask) << 8) |
               static_cast<uint64_t>(data_[offset + 1]);
      return 2;
    }
    case kThreeBytesPacked: {
      *value = ((static_cast<uint64_t>(c) & kMask) << 16) |
               static_cast<uint64_t>(data_[offset + 1]) << 8 |
               static_cast<uint64_t>(data_[offset + 2]);
      return 3;
    }
    case kFourBytesPacked: {
      *value = ((static_cast<uint64_t>(c) & kMask) << 24) |
               static_cast<uint64_t>(data_[offset + 1]) << 16 |
               static_cast<uint64_t>(data_[offset + 2]) << 8 |
               static_cast<uint64_t>(data_[offset + 3]);
      return 4;
    }
    case kFiveBytesPacked: {
      *value = ((static_cast<uint64_t>(c) & kMask) << 32) |
               static_cast<uint64_t>(data_[offset + 1]) << 24 |
               static_cast<uint64_t>(data_[offset + 2]) << 16 |
               static_cast<uint64_t>(data_[offset + 3]) << 8 |
               static_cast<uint64_t>(data_[offset + 4]);
      return 5;
    }
    case kSixBytesPacked: {
      *value = ((static_cast<uint64_t>(c) & kMask) << 40) |
               static_cast<uint64_t>(data_[offset + 1]) << 32 |
               static_cast<uint64_t>(data_[offset + 2]) << 24 |
               static_cast<uint64_t>(data_[offset + 3]) << 16 |
               static_cast<uint64_t>(data_[offset + 4]) << 8 |
               static_cast<uint64_t>(data_[offset + 5]);
      return 6;
    }
    case kSevenBytesPacked: {
      *value = ((static_cast<uint64_t>(c) & kMask) << 48) |
               static_cast<uint64_t>(data_[offset + 1]) << 40 |
               static_cast<uint64_t>(data_[offset + 2]) << 32 |
               static_cast<uint64_t>(data_[offset + 3]) << 24 |
               static_cast<uint64_t>(data_[offset + 4]) << 16 |
               static_cast<uint64_t>(data_[offset + 5]) << 8 |
               static_cast<uint64_t>(data_[offset + 6]);
      return 7;
    }
    default: {
      *value = ((static_cast<uint64_t>(c) & kMask) << 56) |
               static_cast<uint64_t>(data_[offset + 1]) << 48 |
               static_cast<uint64_t>(data_[offset + 2]) << 40 |
               static_cast<uint64_t>(data_[offset + 3]) << 32 |
               static_cast<uint64_t>(data_[offset + 4]) << 24 |
               static_cast<uint64_t>(data_[offset + 5]) << 16 |
               static_cast<uint64_t>(data_[offset + 6]) << 8 |
               static_cast<uint64_t>(data_[offset + 7]);

      return 8;
    }
  }
}

void PackedUintSeq::Restore(std::vector<uint64_t>* vector) const {
  uint64_t prev_value = 0;
  size_t offset = 0;
  uint64_t diff = 0;

  for (size_t i = 0; i < len_; ++i) {
    offset += DeflateSingleInteger(offset, &diff);
    prev_value += diff;

    vector->push_back(prev_value);
  }
}

bool PackedUintSeqIterator::Next(uint64_t* value) {
  if (element_count_ >= parent_.len_) {
    return false;
  }

  uint64_t diff;
  next_offset_ += parent_.DeflateSingleInteger(next_offset_, &diff);
  prev_value_ += diff;

  ++element_count_;
  *value = prev_value_;

  return true;
}

}  // namespace ncode

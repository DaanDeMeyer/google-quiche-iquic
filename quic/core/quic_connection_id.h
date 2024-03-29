// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QUIC_CONNECTION_ID_H_
#define QUICHE_QUIC_CORE_QUIC_CONNECTION_ID_H_

#include <string>
#include <vector>

#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_uint128.h"

namespace quic {

enum QuicConnectionIdLength {
  PACKET_0BYTE_CONNECTION_ID = 0,
  PACKET_8BYTE_CONNECTION_ID = 8,
};

// This is a property of QUIC headers, it indicates whether the connection ID
// should actually be sent over the wire (or was sent on received packets).
enum QuicConnectionIdIncluded : uint8_t {
  CONNECTION_ID_PRESENT = 1,
  CONNECTION_ID_ABSENT = 2,
};

// Connection IDs can be 0-18 bytes per IETF specifications.
const uint8_t kQuicMaxConnectionIdLength = 18;

// kQuicDefaultConnectionIdLength is the only supported length for QUIC
// versions < v99, and is the default picked for all versions.
const uint8_t kQuicDefaultConnectionIdLength = 8;

// According to the IETF spec, the initial server connection ID generated by
// the client must be at least this long.
const uint8_t kQuicMinimumInitialConnectionIdLength = 8;

class QUIC_EXPORT_PRIVATE QuicConnectionId {
 public:
  // Creates a connection ID of length zero.
  QuicConnectionId();

  // Creates a connection ID from network order bytes.
  QuicConnectionId(const char* data, uint8_t length);

  // Creates a connection ID from another connection ID.
  QuicConnectionId(const QuicConnectionId& other);

  // Assignment operator.
  QuicConnectionId& operator=(const QuicConnectionId& other);

  ~QuicConnectionId();

  // Returns the length of the connection ID, in bytes.
  uint8_t length() const;

  // Sets the length of the connection ID, in bytes.
  // WARNING: Calling set_length() can change the in-memory location of the
  // connection ID. Callers must therefore ensure they call data() or
  // mutable_data() after they call set_length().
  void set_length(uint8_t length);

  // Returns a pointer to the connection ID bytes, in network byte order.
  const char* data() const;

  // Returns a mutable pointer to the connection ID bytes,
  // in network byte order.
  char* mutable_data();

  // Returns whether the connection ID has length zero.
  bool IsEmpty() const;

  // Hash() is required to use connection IDs as keys in hash tables.
  size_t Hash() const;

  // Generates an ASCII string that represents
  // the contents of the connection ID, or "0" if it is empty.
  std::string ToString() const;

  // operator<< allows easily logging connection IDs.
  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicConnectionId& v);

  bool operator==(const QuicConnectionId& v) const;
  bool operator!=(const QuicConnectionId& v) const;
  // operator< is required to use connection IDs as keys in hash tables.
  bool operator<(const QuicConnectionId& v) const;

 private:
  uint8_t length_;  // length of the connection ID, in bytes.
  // The connection ID is represented in network byte order.
  union {
    // When quic_use_allocated_connection_ids is false, the connection ID is
    // stored in the first |length_| bytes of |data_|.
    char data_[kQuicMaxConnectionIdLength];
    // When quic_use_allocated_connection_ids is true, if the connection ID
    // fits in |data_short_|, it is stored in the first |length_| bytes of
    // |data_short_|. Otherwise it is stored in |data_long_| which is
    // guaranteed to have a size equal to |length_|. A value of 11 was chosen
    // because our commonly used connection ID length is 8 and with the length,
    // the class is padded to 12 bytes anyway.
    char data_short_[11];
    char* data_long_;
  };
};

// Creates a connection ID of length zero, unless the restart flag
// quic_connection_ids_network_byte_order is false in which case
// it returns an 8-byte all-zeroes connection ID.
QUIC_EXPORT_PRIVATE QuicConnectionId EmptyQuicConnectionId();

// QuicConnectionIdHash can be passed as hash argument to hash tables.
class QuicConnectionIdHash {
 public:
  size_t operator()(QuicConnectionId const& connection_id) const noexcept {
    return connection_id.Hash();
  }
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QUIC_CONNECTION_ID_H_

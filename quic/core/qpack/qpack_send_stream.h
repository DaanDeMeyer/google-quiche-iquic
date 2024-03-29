// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QPACK_QPACK_SEND_STREAM_H_
#define QUICHE_QUIC_CORE_QPACK_QPACK_SEND_STREAM_H_

#include <cstdint>

#include "net/third_party/quiche/src/quic/core/qpack/qpack_stream_sender_delegate.h"
#include "net/third_party/quiche/src/quic/core/quic_stream.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

class QuicSpdySession;

// QPACK 4.2.1 Encoder and Decoder Streams.
// The QPACK send stream is self initiated and is write only.
class QUIC_EXPORT_PRIVATE QpackSendStream : public QuicStream,
                                            public QpackStreamSenderDelegate {
 public:
  // |session| can't be nullptr, and the ownership is not passed. |session| owns
  // this stream.
  QpackSendStream(QuicStreamId id,
                  QuicSpdySession* session,
                  uint64_t stream_type);
  QpackSendStream(const QpackSendStream&) = delete;
  QpackSendStream& operator=(const QpackSendStream&) = delete;
  ~QpackSendStream() override = default;

  // Overriding QuicStream::OnStreamReset to make sure QPACK stream is
  // never closed before connection.
  void OnStreamReset(const QuicRstStreamFrame& frame) override;

  // The send QPACK stream is write unidirectional, so this method
  // should never be called.
  void OnDataAvailable() override { QUIC_NOTREACHED(); }

  // Writes the instructions to peer. The stream type will be sent
  // before the first instruction so that the peer can open an qpack stream.
  void WriteStreamData(QuicStringPiece data) override;

 private:
  const uint64_t stream_type_;
  bool stream_type_sent_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QPACK_QPACK_SEND_STREAM_H_

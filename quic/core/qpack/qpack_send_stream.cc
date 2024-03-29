// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/qpack/qpack_send_stream.h"

#include "net/third_party/quiche/src/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_arraysize.h"

namespace quic {
QpackSendStream::QpackSendStream(QuicStreamId id,
                                 QuicSpdySession* session,
                                 uint64_t stream_type)
    : QuicStream(id, session, /*is_static = */ true, WRITE_UNIDIRECTIONAL),
      stream_type_(stream_type),
      stream_type_sent_(false) {}

void QpackSendStream::OnStreamReset(const QuicRstStreamFrame& /*frame*/) {
  // TODO(renjietang) Change the error code to H/3 specific
  // HTTP_CLOSED_CRITICAL_STREAM.
  session()->connection()->CloseConnection(
      QUIC_INVALID_STREAM_ID, "Attempt to reset qpack send stream",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

void QpackSendStream::WriteStreamData(QuicStringPiece data) {
  QuicConnection::ScopedPacketFlusher flusher(session()->connection());
  if (!stream_type_sent_) {
    char type[sizeof(stream_type_)];
    QuicDataWriter writer(QUIC_ARRAYSIZE(type), type);
    writer.WriteVarInt62(stream_type_);
    WriteOrBufferData(QuicStringPiece(writer.data(), writer.length()), false,
                      nullptr);
    stream_type_sent_ = true;
  }
  WriteOrBufferData(data, false, nullptr);
}

}  // namespace quic

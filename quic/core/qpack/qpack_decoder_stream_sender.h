// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QPACK_QPACK_DECODER_STREAM_SENDER_H_
#define QUICHE_QUIC_CORE_QPACK_QPACK_DECODER_STREAM_SENDER_H_

#include <cstdint>

#include "net/third_party/quiche/src/quic/core/qpack/qpack_instruction_encoder.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_stream_sender_delegate.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

// This class serializes (encodes) instructions for transmission on the decoder
// stream.
class QUIC_EXPORT_PRIVATE QpackDecoderStreamSender {
 public:
  explicit QpackDecoderStreamSender(QpackStreamSenderDelegate* delegate);
  QpackDecoderStreamSender() = delete;
  QpackDecoderStreamSender(const QpackDecoderStreamSender&) = delete;
  QpackDecoderStreamSender& operator=(const QpackDecoderStreamSender&) = delete;

  // Methods for sending instructions, see
  // https://quicwg.org/base-drafts/draft-ietf-quic-qpack.html#rfc.section.5.3

  // 5.3.1 Insert Count Increment
  void SendInsertCountIncrement(uint64_t increment);
  // 5.3.2 Header Acknowledgement
  void SendHeaderAcknowledgement(QuicStreamId stream_id);
  // 5.3.3 Stream Cancellation
  void SendStreamCancellation(QuicStreamId stream_id);

 private:
  QpackStreamSenderDelegate* const delegate_;
  QpackInstructionEncoder instruction_encoder_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QPACK_QPACK_DECODER_STREAM_SENDER_H_

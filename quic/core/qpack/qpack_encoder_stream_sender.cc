// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder_stream_sender.h"

#include <cstddef>
#include <limits>
#include <string>

#include "net/third_party/quiche/src/quic/core/qpack/qpack_constants.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"

namespace quic {

QpackEncoderStreamSender::QpackEncoderStreamSender(
    QpackStreamSenderDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

void QpackEncoderStreamSender::SendInsertWithNameReference(
    bool is_static,
    uint64_t name_index,
    QuicStringPiece value) {
  instruction_encoder_.set_s_bit(is_static);
  instruction_encoder_.set_varint(name_index);
  instruction_encoder_.set_value(value);

  std::string output;
  instruction_encoder_.Encode(InsertWithNameReferenceInstruction(), &output);
  delegate_->WriteStreamData(output);
}

void QpackEncoderStreamSender::SendInsertWithoutNameReference(
    QuicStringPiece name,
    QuicStringPiece value) {
  instruction_encoder_.set_name(name);
  instruction_encoder_.set_value(value);

  std::string output;
  instruction_encoder_.Encode(InsertWithoutNameReferenceInstruction(), &output);
  delegate_->WriteStreamData(output);
}

void QpackEncoderStreamSender::SendDuplicate(uint64_t index) {
  instruction_encoder_.set_varint(index);

  std::string output;
  instruction_encoder_.Encode(DuplicateInstruction(), &output);
  delegate_->WriteStreamData(output);
}

void QpackEncoderStreamSender::SendSetDynamicTableCapacity(uint64_t capacity) {
  instruction_encoder_.set_varint(capacity);

  std::string output;
  instruction_encoder_.Encode(SetDynamicTableCapacityInstruction(), &output);
  delegate_->WriteStreamData(output);
}

}  // namespace quic

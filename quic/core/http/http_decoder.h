// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_HTTP_HTTP_DECODER_H_
#define QUICHE_QUIC_CORE_HTTP_HTTP_DECODER_H_

#include "net/third_party/quiche/src/quic/core/http/http_frames.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

namespace test {

class HttpDecoderPeer;

}  // namespace test

class QuicDataReader;

// Struct that stores meta data of an HTTP/3 frame.
// |header_length| is frame header length in bytes.
// |payload_length| is frame payload length in bytes.
struct QUIC_EXPORT_PRIVATE Http3FrameLengths {
  Http3FrameLengths(QuicByteCount header, QuicByteCount payload)
      : header_length(header), payload_length(payload) {}

  bool operator==(const Http3FrameLengths& other) const {
    return (header_length == other.header_length) &&
           (payload_length == other.payload_length);
  }

  QuicByteCount header_length;
  QuicByteCount payload_length;
};

// A class for decoding the HTTP frames that are exchanged in an HTTP over QUIC
// session.
class QUIC_EXPORT_PRIVATE HttpDecoder {
 public:
  class QUIC_EXPORT_PRIVATE Visitor {
   public:
    virtual ~Visitor() {}

    // Called if an error is detected.
    virtual void OnError(HttpDecoder* decoder) = 0;

    // All the following methods return true to continue decoding,
    // and false to pause it.

    // Called when a PRIORITY frame has been received.
    // |frame_length| contains PRIORITY frame length and payload length.
    virtual bool OnPriorityFrameStart(Http3FrameLengths frame_length) = 0;

    // Called when a PRIORITY frame has been successfully parsed.
    virtual bool OnPriorityFrame(const PriorityFrame& frame) = 0;

    // Called when a CANCEL_PUSH frame has been successfully parsed.
    virtual bool OnCancelPushFrame(const CancelPushFrame& frame) = 0;

    // Called when a MAX_PUSH_ID frame has been successfully parsed.
    virtual bool OnMaxPushIdFrame(const MaxPushIdFrame& frame) = 0;

    // Called when a GOAWAY frame has been successfully parsed.
    virtual bool OnGoAwayFrame(const GoAwayFrame& frame) = 0;

    // Called when a SETTINGS frame has been received.
    virtual bool OnSettingsFrameStart(Http3FrameLengths frame_length) = 0;

    // Called when a SETTINGS frame has been successfully parsed.
    virtual bool OnSettingsFrame(const SettingsFrame& frame) = 0;

    // Called when a DUPLICATE_PUSH frame has been successfully parsed.
    virtual bool OnDuplicatePushFrame(const DuplicatePushFrame& frame) = 0;

    // Called when a DATA frame has been received.
    // |frame_length| contains DATA frame length and payload length.
    virtual bool OnDataFrameStart(Http3FrameLengths frame_length) = 0;
    // Called when part of the payload of a DATA frame has been read.  May be
    // called multiple times for a single frame.  |payload| is guaranteed to be
    // non-empty.
    virtual bool OnDataFramePayload(QuicStringPiece payload) = 0;
    // Called when a DATA frame has been completely processed.
    virtual bool OnDataFrameEnd() = 0;

    // Called when a HEADERS frame has been received.
    // |frame_length| contains HEADERS frame length and payload length.
    virtual bool OnHeadersFrameStart(Http3FrameLengths frame_length) = 0;
    // Called when part of the payload of a HEADERS frame has been read.  May be
    // called multiple times for a single frame.  |payload| is guaranteed to be
    // non-empty.
    virtual bool OnHeadersFramePayload(QuicStringPiece payload) = 0;
    // Called when a HEADERS frame has been completely processed.
    // |frame_len| is the length of the HEADERS frame payload.
    virtual bool OnHeadersFrameEnd() = 0;

    // Called when a PUSH_PROMISE frame has been received for |push_id|.
    virtual bool OnPushPromiseFrameStart(PushId push_id) = 0;
    // Called when part of the payload of a PUSH_PROMISE frame has been read.
    // May be called multiple times for a single frame.  |payload| is guaranteed
    // to be non-empty.
    virtual bool OnPushPromiseFramePayload(QuicStringPiece payload) = 0;
    // Called when a PUSH_PROMISE frame has been completely processed.
    virtual bool OnPushPromiseFrameEnd() = 0;

    // TODO(rch): Consider adding methods like:
    // OnUnknownFrame{Start,Payload,End}()
    // to allow callers to handle unknown frames.
  };

  // |visitor| must be non-null, and must outlive HttpDecoder.
  explicit HttpDecoder(Visitor* visitor);

  ~HttpDecoder();

  // Processes the input and invokes the appropriate visitor methods, until a
  // visitor method returns false or an error occurs.  Returns the number of
  // bytes processed.  Does not process any input if called after an error.
  // Paused processing can be resumed by calling ProcessInput() again with the
  // unprocessed portion of data.  Must not be called after an error has
  // occurred.
  QuicByteCount ProcessInput(const char* data, QuicByteCount len);

  // Returns an error code other than QUIC_NO_ERROR if and only if
  // Visitor::OnError() has been called.
  QuicErrorCode error() const { return error_; }

  const std::string& error_detail() const { return error_detail_; }

 private:
  friend test::HttpDecoderPeer;

  // Represents the current state of the parsing state machine.
  enum HttpDecoderState {
    STATE_READING_FRAME_LENGTH,
    STATE_READING_FRAME_TYPE,
    STATE_READING_FRAME_PAYLOAD,
    STATE_FINISH_PARSING,
    STATE_ERROR
  };

  // Reads the type of a frame from |reader|. Sets error_ and error_detail_
  // if there are any errors.  Also calls OnDataFrameStart() or
  // OnHeadersFrameStart() for appropriate frame types.
  void ReadFrameType(QuicDataReader* reader);

  // Reads the length of a frame from |reader|. Sets error_ and error_detail_
  // if there are any errors.  Returns whether processing should continue.
  bool ReadFrameLength(QuicDataReader* reader);

  // Reads the payload of the current frame from |reader| and processes it,
  // possibly buffering the data or invoking the visitor.  Returns whether
  // processing should continue.
  bool ReadFramePayload(QuicDataReader* reader);

  // Optionally parses buffered data; calls visitor method to signal that frame
  // had been parsed completely.  Returns whether processing should continue.
  bool FinishParsing();

  // Discards any remaining frame payload from |reader|.
  void DiscardFramePayload(QuicDataReader* reader);

  // Buffers any remaining frame payload from |reader| into |buffer_|.
  void BufferFramePayload(QuicDataReader* reader);

  // Buffers any remaining frame length field from |reader| into
  // |length_buffer_|.
  void BufferFrameLength(QuicDataReader* reader);

  // Buffers any remaining frame type field from |reader| into |type_buffer_|.
  void BufferFrameType(QuicDataReader* reader);

  // Sets |error_| and |error_detail_| accordingly.
  void RaiseError(QuicErrorCode error, std::string error_detail);

  // Parses the payload of a PRIORITY frame from |reader| into |frame|.
  bool ParsePriorityFrame(QuicDataReader* reader, PriorityFrame* frame);

  // Parses the payload of a SETTINGS frame from |reader| into |frame|.
  bool ParseSettingsFrame(QuicDataReader* reader, SettingsFrame* frame);

  // Returns the max frame size of a given |frame_type|.
  QuicByteCount MaxFrameLength(uint8_t frame_type);

  // Visitor to invoke when messages are parsed.
  Visitor* const visitor_;  // Unowned.
  // Current state of the parsing.
  HttpDecoderState state_;
  // Type of the frame currently being parsed.
  uint64_t current_frame_type_;
  // Size of the frame's length field.
  QuicByteCount current_length_field_length_;
  // Remaining length that's needed for the frame's length field.
  QuicByteCount remaining_length_field_length_;
  // Length of the payload of the frame currently being parsed.
  QuicByteCount current_frame_length_;
  // Remaining payload bytes to be parsed.
  QuicByteCount remaining_frame_length_;
  // Length of the frame's type field.
  QuicByteCount current_type_field_length_;
  // Remaining length that's needed for the frame's type field.
  QuicByteCount remaining_type_field_length_;
  // Last error.
  QuicErrorCode error_;
  // The issue which caused |error_|
  std::string error_detail_;
  // Remaining unparsed data.
  std::string buffer_;
  // Remaining unparsed length field data.
  std::array<char, sizeof(uint64_t)> length_buffer_;
  // Remaining unparsed type field data.
  std::array<char, sizeof(uint64_t)> type_buffer_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_HTTP_HTTP_DECODER_H_

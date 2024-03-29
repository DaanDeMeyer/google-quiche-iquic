// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/http/http_decoder.h"

#include "net/third_party/quiche/src/quic/core/http/http_encoder.h"
#include "net/third_party/quiche/src/quic/core/quic_data_writer.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace quic {

namespace test {

class HttpDecoderPeer {
 public:
  static uint64_t current_frame_type(HttpDecoder* decoder) {
    return decoder->current_frame_type_;
  }
};

class MockVisitor : public HttpDecoder::Visitor {
 public:
  ~MockVisitor() override = default;

  // Called if an error is detected.
  MOCK_METHOD1(OnError, void(HttpDecoder* decoder));

  MOCK_METHOD1(OnPriorityFrameStart, bool(Http3FrameLengths frame_lengths));
  MOCK_METHOD1(OnPriorityFrame, bool(const PriorityFrame& frame));
  MOCK_METHOD1(OnCancelPushFrame, bool(const CancelPushFrame& frame));
  MOCK_METHOD1(OnMaxPushIdFrame, bool(const MaxPushIdFrame& frame));
  MOCK_METHOD1(OnGoAwayFrame, bool(const GoAwayFrame& frame));
  MOCK_METHOD1(OnSettingsFrameStart, bool(Http3FrameLengths frame_lengths));
  MOCK_METHOD1(OnSettingsFrame, bool(const SettingsFrame& frame));
  MOCK_METHOD1(OnDuplicatePushFrame, bool(const DuplicatePushFrame& frame));

  MOCK_METHOD1(OnDataFrameStart, bool(Http3FrameLengths frame_lengths));
  MOCK_METHOD1(OnDataFramePayload, bool(QuicStringPiece payload));
  MOCK_METHOD0(OnDataFrameEnd, bool());

  MOCK_METHOD1(OnHeadersFrameStart, bool(Http3FrameLengths frame_lengths));
  MOCK_METHOD1(OnHeadersFramePayload, bool(QuicStringPiece payload));
  MOCK_METHOD0(OnHeadersFrameEnd, bool());

  MOCK_METHOD1(OnPushPromiseFrameStart, bool(PushId push_id));
  MOCK_METHOD1(OnPushPromiseFramePayload, bool(QuicStringPiece payload));
  MOCK_METHOD0(OnPushPromiseFrameEnd, bool());
};

class HttpDecoderTest : public QuicTest {
 public:
  HttpDecoderTest() : decoder_(&visitor_) {
    ON_CALL(visitor_, OnPriorityFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnPriorityFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnCancelPushFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnMaxPushIdFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnGoAwayFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnSettingsFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnSettingsFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnDuplicatePushFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnDataFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnDataFramePayload(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnDataFrameEnd()).WillByDefault(Return(true));
    ON_CALL(visitor_, OnHeadersFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnHeadersFramePayload(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnHeadersFrameEnd()).WillByDefault(Return(true));
    ON_CALL(visitor_, OnPushPromiseFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnPushPromiseFramePayload(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnPushPromiseFrameEnd()).WillByDefault(Return(true));
  }
  ~HttpDecoderTest() override = default;

  uint64_t current_frame_type() {
    return HttpDecoderPeer::current_frame_type(&decoder_);
  }

  // Process |input| in a single call to HttpDecoder::ProcessInput().
  QuicByteCount ProcessInput(QuicStringPiece input) {
    return decoder_.ProcessInput(input.data(), input.size());
  }

  // Feed |input| to |decoder_| one character at a time,
  // verifying that each character gets processed.
  void ProcessInputCharByChar(QuicStringPiece input) {
    for (char c : input) {
      EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
    }
  }

  // Append garbage to |input|, then process it in a single call to
  // HttpDecoder::ProcessInput().  Verify that garbage is not read.
  QuicByteCount ProcessInputWithGarbageAppended(QuicStringPiece input) {
    std::string input_with_garbage_appended = QuicStrCat(input, "blahblah");
    QuicByteCount processed_bytes = ProcessInput(input_with_garbage_appended);

    // Guaranteed by HttpDecoder::ProcessInput() contract.
    DCHECK_LE(processed_bytes, input_with_garbage_appended.size());

    // Caller should set up visitor to pause decoding
    // before HttpDecoder would read garbage.
    EXPECT_LE(processed_bytes, input.size());

    return processed_bytes;
  }

  testing::StrictMock<MockVisitor> visitor_;
  HttpDecoder decoder_;
};

TEST_F(HttpDecoderTest, InitialState) {
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, ReservedFramesNoPayload) {
  std::unique_ptr<char[]> input;
  for (int n = 0; n < 8; ++n) {
    const uint8_t type = 0xB + 0x1F * n;
    QuicByteCount total_length = QuicDataWriter::GetVarInt62Len(0x00) +
                                 QuicDataWriter::GetVarInt62Len(type);
    input = QuicMakeUnique<char[]>(total_length);
    QuicDataWriter writer(total_length, input.get());
    writer.WriteVarInt62(type);
    writer.WriteVarInt62(0x00);

    EXPECT_EQ(total_length, decoder_.ProcessInput(input.get(), total_length))
        << n;
    EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
    ASSERT_EQ("", decoder_.error_detail());
    EXPECT_EQ(type, current_frame_type());
  }
  // Test on a arbitrary reserved frame with 2-byte type field by hard coding
  // variable length integer.
  char in[] = {// type 0xB + 0x1F*3
               0x40, 0x68,
               // length
               0x00};
  EXPECT_EQ(3u, decoder_.ProcessInput(in, QUIC_ARRAYSIZE(in)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  ASSERT_EQ("", decoder_.error_detail());
  EXPECT_EQ(0xB + 0x1F * 3u, current_frame_type());
}

TEST_F(HttpDecoderTest, ReservedFramesSmallPayload) {
  std::unique_ptr<char[]> input;
  const uint8_t payload_size = 50;
  std::string data(payload_size, 'a');
  for (int n = 0; n < 8; ++n) {
    const uint8_t type = 0xB + 0x1F * n;
    QuicByteCount total_length = QuicDataWriter::GetVarInt62Len(payload_size) +
                                 QuicDataWriter::GetVarInt62Len(type) +
                                 payload_size;
    input = QuicMakeUnique<char[]>(total_length);
    QuicDataWriter writer(total_length, input.get());
    writer.WriteVarInt62(type);
    writer.WriteVarInt62(payload_size);
    writer.WriteStringPiece(data);
    EXPECT_EQ(total_length, decoder_.ProcessInput(input.get(), total_length))
        << n;
    EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
    ASSERT_EQ("", decoder_.error_detail());
    EXPECT_EQ(type, current_frame_type());
  }

  // Test on a arbitrary reserved frame with 2-byte type field by hard coding
  // variable length integer.
  char in[payload_size + 3] = {// type 0xB + 0x1F*3
                               0x40, 0x68,
                               // length
                               payload_size};
  EXPECT_EQ(QUIC_ARRAYSIZE(in), decoder_.ProcessInput(in, QUIC_ARRAYSIZE(in)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  ASSERT_EQ("", decoder_.error_detail());
  EXPECT_EQ(0xB + 0x1F * 3u, current_frame_type());
}

TEST_F(HttpDecoderTest, ReservedFramesLargePayload) {
  std::unique_ptr<char[]> input;
  const QuicByteCount payload_size = 256;
  std::string data(payload_size, 'a');
  for (int n = 0; n < 8; ++n) {
    const uint8_t type = 0xB + 0x1F * n;
    QuicByteCount total_length = QuicDataWriter::GetVarInt62Len(payload_size) +
                                 QuicDataWriter::GetVarInt62Len(type) +
                                 payload_size;
    input = QuicMakeUnique<char[]>(total_length);
    QuicDataWriter writer(total_length, input.get());
    writer.WriteVarInt62(type);
    writer.WriteVarInt62(payload_size);
    writer.WriteStringPiece(data);

    EXPECT_EQ(total_length, decoder_.ProcessInput(input.get(), total_length))
        << n;
    EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
    ASSERT_EQ("", decoder_.error_detail());
    EXPECT_EQ(type, current_frame_type());
  }

  // Test on a arbitrary reserved frame with 2-byte type field by hard coding
  // variable length integer.
  char in[payload_size + 4] = {// type 0xB + 0x1F*3
                               0x40, 0x68,
                               // length
                               0x40 + 0x01, 0x00};
  EXPECT_EQ(QUIC_ARRAYSIZE(in), decoder_.ProcessInput(in, QUIC_ARRAYSIZE(in)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  ASSERT_EQ("", decoder_.error_detail());
  EXPECT_EQ(0xB + 0x1F * 3u, current_frame_type());
}

TEST_F(HttpDecoderTest, CancelPush) {
  InSequence s;
  std::string input =
      "\x03"   // type (CANCEL_PUSH)
      "\x01"   // length
      "\x01";  // Push Id

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnCancelPushFrame(CancelPushFrame({1})))
      .WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnCancelPushFrame(CancelPushFrame({1})));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnCancelPushFrame(CancelPushFrame({1})));
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PushPromiseFrame) {
  InSequence s;
  std::string input =
      "\x05"      // type (PUSH_PROMISE)
      "\x08"      // length
      "\x01"      // Push Id
      "Headers";  // Header Block

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1)).WillOnce(Return(false));
  QuicStringPiece remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(3u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("Headers")))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("Headers")));
  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("H")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("d")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("r")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("s")));
  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, MaxPushId) {
  InSequence s;
  std::string input =
      "\x0D"   // type (MAX_PUSH_ID)
      "\x01"   // length
      "\x01";  // Push Id

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame(MaxPushIdFrame({1})))
      .WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame(MaxPushIdFrame({1})));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame(MaxPushIdFrame({1})));
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, DuplicatePush) {
  InSequence s;
  std::string input =
      "\x0E"   // type (DUPLICATE_PUSH)
      "\x01"   // length
      "\x01";  // Push Id

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnDuplicatePushFrame(DuplicatePushFrame({1})))
      .WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnDuplicatePushFrame(DuplicatePushFrame({1})));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnDuplicatePushFrame(DuplicatePushFrame({1})));
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PriorityFrame) {
  InSequence s;
  std::string input =
      "\x02"   // type (PRIORITY)
      "\x04"   // length
      "\x01"   // request stream, request stream, exclusive
      "\x03"   // prioritized_element_id
      "\x04"   // element_dependency_id
      "\xFF";  // weight

  PriorityFrame frame;
  frame.prioritized_type = REQUEST_STREAM;
  frame.dependency_type = REQUEST_STREAM;
  frame.exclusive = true;
  frame.prioritized_element_id = 0x03;
  frame.element_dependency_id = 0x04;
  frame.weight = 0xFF;

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnPriorityFrameStart(Http3FrameLengths(2, 4)))
      .WillOnce(Return(false));
  QuicStringPiece remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnPriorityFrame(frame)).WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnPriorityFrameStart(Http3FrameLengths(2, 4)));
  EXPECT_CALL(visitor_, OnPriorityFrame(frame));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnPriorityFrameStart(Http3FrameLengths(2, 4)));
  EXPECT_CALL(visitor_, OnPriorityFrame(frame));
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  std::string input2 =
      "\x02"   // type (PRIORITY)
      "\x02"   // length
      "\xf1"   // root of tree, root of tree, exclusive
      "\xFF";  // weight
  PriorityFrame frame2;
  frame2.prioritized_type = ROOT_OF_TREE;
  frame2.dependency_type = ROOT_OF_TREE;
  frame2.exclusive = true;
  frame2.weight = 0xFF;

  EXPECT_CALL(visitor_, OnPriorityFrameStart(Http3FrameLengths(2, 2)));
  EXPECT_CALL(visitor_, OnPriorityFrame(frame2));
  EXPECT_EQ(input2.size(), ProcessInput(input2));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, SettingsFrame) {
  InSequence s;
  std::string input(
      "\x04"      // type (SETTINGS)
      "\x07"      // length
      "\x03"      // identifier (SETTINGS_NUM_PLACEHOLDERS)
      "\x02"      // content
      "\x06"      // identifier (SETTINGS_MAX_HEADER_LIST_SIZE)
      "\x05"      // content
      "\x41\x00"  // identifier, encoded on 2 bytes (0x40), value is 256 (0x100)
      "\x04",     // content
      9);         // length of string

  SettingsFrame frame;
  frame.values[3] = 2;
  frame.values[6] = 5;
  frame.values[256] = 4;

  // Visitor pauses processing.
  QuicStringPiece remaining_input(input);
  EXPECT_CALL(visitor_, OnSettingsFrameStart(Http3FrameLengths(2, 7)))
      .WillOnce(Return(false));
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnSettingsFrame(frame)).WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnSettingsFrameStart(Http3FrameLengths(2, 7)));
  EXPECT_CALL(visitor_, OnSettingsFrame(frame));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnSettingsFrameStart(Http3FrameLengths(2, 7)));
  EXPECT_CALL(visitor_, OnSettingsFrame(frame));
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, DataFrame) {
  InSequence s;
  std::string input(
      "\x00"    // type (DATA)
      "\x05"    // length
      "Data!",  // data
      7);

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(2, 5)))
      .WillOnce(Return(false));
  QuicStringPiece remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("Data!")))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_CALL(visitor_, OnDataFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(2, 5)));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("Data!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(2, 5)));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("D")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("t")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, FrameHeaderPartialDelivery) {
  InSequence s;
  // A large input that will occupy more than 1 byte in the length field.
  std::string input(2048, 'x');
  HttpEncoder encoder;
  std::unique_ptr<char[]> buffer;
  QuicByteCount header_length =
      encoder.SerializeDataFrameHeader(input.length(), &buffer);
  std::string header = std::string(buffer.get(), header_length);
  // Partially send only 1 byte of the header to process.
  EXPECT_EQ(1u, decoder_.ProcessInput(header.data(), 1));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Send the rest of the header.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(3, 2048)));
  EXPECT_EQ(header_length - 1,
            decoder_.ProcessInput(header.data() + 1, header_length - 1));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Send data.
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece(input)));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  EXPECT_EQ(2048u, decoder_.ProcessInput(input.data(), 2048));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PartialDeliveryOfLargeFrameType) {
  // Use a reserved type that's more than 1 byte in VarInt62.
  const uint8_t type = 0xB + 0x1F * 3;
  std::unique_ptr<char[]> input;
  QuicByteCount total_length = QuicDataWriter::GetVarInt62Len(0x00) +
                               QuicDataWriter::GetVarInt62Len(type);
  input.reset(new char[total_length]);
  QuicDataWriter writer(total_length, input.get());
  writer.WriteVarInt62(type);
  writer.WriteVarInt62(0x00);

  auto raw_input = input.get();
  for (uint64_t i = 0; i < total_length; ++i) {
    char c = raw_input[i];
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
  EXPECT_EQ(type, current_frame_type());
}

TEST_F(HttpDecoderTest, GoAway) {
  InSequence s;
  std::string input =
      "\x07"   // type (GOAWAY)
      "\x01"   // length
      "\x01";  // StreamId

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})))
      .WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})));
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, HeadersFrame) {
  InSequence s;
  std::string input =
      "\x01"      // type (HEADERS)
      "\x07"      // length
      "Headers";  // headers

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(Http3FrameLengths(2, 7)))
      .WillOnce(Return(false));
  QuicStringPiece remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("Headers")))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_CALL(visitor_, OnHeadersFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(Http3FrameLengths(2, 7)));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("Headers")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(Http3FrameLengths(2, 7)));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("H")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("d")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("r")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("s")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, EmptyDataFrame) {
  InSequence s;
  std::string input(
      "\x00"   // type (DATA)
      "\x00",  // length
      2);

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(2, 0)))
      .WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));

  EXPECT_CALL(visitor_, OnDataFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(2, 0)));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(2, 0)));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, EmptyHeadersFrame) {
  InSequence s;
  std::string input(
      "\x01"   // type (HEADERS)
      "\x00",  // length
      2);

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(Http3FrameLengths(2, 0)))
      .WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));

  EXPECT_CALL(visitor_, OnHeadersFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(Http3FrameLengths(2, 0)));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(Http3FrameLengths(2, 0)));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PushPromiseFrameNoHeaders) {
  InSequence s;
  std::string input =
      "\x05"   // type (PUSH_PROMISE)
      "\x01"   // length
      "\x01";  // Push Id

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1)).WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));

  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1));
  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1));
  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, MalformedFrameWithOverlyLargePayload) {
  std::string input =
      "\x03"   // type (CANCEL_PUSH)
      "\x10"   // length
      "\x15";  // malformed payload
  // Process the full frame.
  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(2u, ProcessInput(input));
  EXPECT_EQ(QUIC_INTERNAL_ERROR, decoder_.error());
  EXPECT_EQ("Frame is too large", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, MalformedSettingsFrame) {
  char input[30];
  QuicDataWriter writer(30, input);
  // Write type SETTINGS.
  writer.WriteUInt8(0x04);
  // Write length.
  writer.WriteVarInt62(2048 * 1024);

  writer.WriteStringPiece("Malformed payload");
  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(5u, decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_INTERNAL_ERROR, decoder_.error());
  EXPECT_EQ("Frame is too large", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, HeadersPausedThenData) {
  InSequence s;
  std::string input(
      "\x01"     // type (HEADERS)
      "\x07"     // length
      "Headers"  // headers
      "\x00"     // type (DATA)
      "\x05"     // length
      "Data!",   // data
      16);

  // Visitor pauses processing, maybe because header decompression is blocked.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(Http3FrameLengths(2, 7)));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("Headers")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd()).WillOnce(Return(false));
  QuicStringPiece remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(9u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  // Process DATA frame.
  EXPECT_CALL(visitor_, OnDataFrameStart(Http3FrameLengths(2, 5)));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("Data!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());

  processed_bytes = ProcessInput(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

}  // namespace test

}  // namespace quic

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <string>

#include "net/third_party/quiche/src/quic/core/qpack/qpack_decoder_test_utils.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_encoder_test_utils.h"
#include "net/third_party/quiche/src/quic/core/qpack/qpack_utils.h"
#include "net/third_party/quiche/src/quic/core/qpack/value_splitting_header_list.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_fuzzed_data_provider.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"

namespace quic {
namespace test {

// This fuzzer exercises QpackEncoder and QpackDecoder.  It should be able to
// cover all possible code paths of QpackEncoder.  However, since the resulting
// header block is always valid and is encoded in a particular way, this fuzzer
// is not expected to cover all code paths of QpackDecoder.  On the other hand,
// encoding then decoding is expected to result in the original header list, and
// this fuzzer checks for that.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  QuicFuzzedDataProvider provider(data, size);

  // Build test header list.
  spdy::SpdyHeaderBlock header_list;
  uint8_t header_count = provider.ConsumeIntegral<uint8_t>();
  for (uint8_t header_index = 0; header_index < header_count; ++header_index) {
    if (provider.remaining_bytes() == 0) {
      // Do not add more headers if there is no more fuzzer data.
      break;
    }

    std::string name;
    std::string value;
    switch (provider.ConsumeIntegral<uint8_t>()) {
      case 0:
        // Static table entry with no header value.
        name = ":authority";
        break;
      case 1:
        // Static table entry with no header value, using non-empty header
        // value.
        name = ":authority";
        value = "www.example.org";
        break;
      case 2:
        // Static table entry with header value, using that header value.
        name = ":accept-encoding";
        value = "gzip, deflate";
        break;
      case 3:
        // Static table entry with header value, using empty header value.
        name = ":accept-encoding";
        break;
      case 4:
        // Static table entry with header value, using different, non-empty
        // header value.
        name = ":accept-encoding";
        value = "brotli";
        break;
      case 5:
        // Header name that has multiple entries in the static table,
        // using header value from one of them.
        name = ":method";
        value = "GET";
        break;
      case 6:
        // Header name that has multiple entries in the static table,
        // using empty header value.
        name = ":method";
        break;
      case 7:
        // Header name that has multiple entries in the static table,
        // using different, non-empty header value.
        name = ":method";
        value = "CONNECT";
        break;
      case 8:
        // Header name not in the static table, empty header value.
        name = "foo";
        value = "";
        break;
      case 9:
        // Header name not in the static table, non-empty fixed header value.
        name = "foo";
        value = "bar";
        break;
      case 10:
        // Header name not in the static table, fuzzed header value.
        name = "foo";
        value = provider.ConsumeRandomLengthString(128);
        break;
      case 11:
        // Another header name not in the static table, empty header value.
        name = "bar";
        value = "";
        break;
      case 12:
        // Another header name not in the static table, non-empty fixed header
        // value.
        name = "bar";
        value = "baz";
        break;
      case 13:
        // Another header name not in the static table, fuzzed header value.
        name = "bar";
        value = provider.ConsumeRandomLengthString(128);
        break;
      default:
        // Fuzzed header name and header value.
        name = provider.ConsumeRandomLengthString(128);
        value = provider.ConsumeRandomLengthString(128);
    }

    header_list.AppendValueOrAddHeader(name, value);
  }

  // Process up to 64 kB fragments at a time.  Too small upper bound might not
  // provide enough coverage, too large would make fuzzing less efficient.
  auto fragment_size_generator =
      std::bind(&QuicFuzzedDataProvider::ConsumeIntegralInRange<uint16_t>,
                &provider, 1, std::numeric_limits<uint16_t>::max());

  // Encode header list.
  NoopDecoderStreamErrorDelegate decoder_stream_error_delegate;
  NoopQpackStreamSenderDelegate encoder_stream_sender_delegate;
  QpackEncoder encoder(&decoder_stream_error_delegate,
                       &encoder_stream_sender_delegate);
  std::string encoded_header_block =
      encoder.EncodeHeaderList(/* stream_id = */ 1, &header_list);

  // Decode header block.
  TestHeadersHandler handler;
  NoopEncoderStreamErrorDelegate encoder_stream_error_delegate;
  NoopQpackStreamSenderDelegate decoder_stream_sender_delegate;
  QpackDecode(&encoder_stream_error_delegate, &decoder_stream_sender_delegate,
              &handler, fragment_size_generator, encoded_header_block);

  // Since header block has been produced by encoding a header list, it must be
  // valid.
  CHECK(handler.decoding_completed());
  CHECK(!handler.decoding_error_detected());

  // Encoder splits |header_list| header values along '\0' or ';' separators.
  // Do the same here so that we get matching results.
  ValueSplittingHeaderList splitting_header_list(&header_list);
  spdy::SpdyHeaderBlock expected_header_list;
  for (const auto& header : splitting_header_list) {
    expected_header_list.AppendValueOrAddHeader(header.first, header.second);
  }
  // Compare resulting header list to original.
  CHECK(expected_header_list == handler.ReleaseHeaderList());

  return 0;
}

}  // namespace test
}  // namespace quic

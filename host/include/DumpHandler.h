#ifndef SERCOM_DUMP_HANDLER_H
#define SERCOM_DUMP_HANDLER_H

#include "../../core/include/Definitions.h"
#include "../../core/include/Frame.h"
#include "../../core/include/FrameDecoder.h"

namespace sercom::DumpHandler {

class DumpHandler final : public FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Printer of every decoded frame and every decode error of a capture.
     */
    DumpHandler() = default;
    ~DumpHandler() final;

    // delete copy constructor and copy assignment operator
    DumpHandler(const DumpHandler &) = delete;
    DumpHandler &operator=(const DumpHandler &) = delete;

    // delete move constructor and move assignment operator
    DumpHandler(DumpHandler &&) = delete;
    DumpHandler &operator=(DumpHandler &&) = delete;

public:
    /*
     * Listen for a received frame and print it on one line.
     */
    void onReceivedFrame(const Frame::Frame &frame) final;

    /*
     * Listen for a decode error and print it on one line.
     */
    void onDecodeError(Definitions::DecodeErrorType decodeErrorType) final;
};

} // namespace sercom::DumpHandler

#endif // SERCOM_DUMP_HANDLER_H

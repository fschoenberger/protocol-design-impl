#pragma once

#include <boost/circular_buffer.hpp>
#include <unordered_map>

#include "logger.hpp"

namespace rft {

namespace CongestionControl {

//using input_channel = boost::asio::experimental::channel<void(boost::system::error_code, std::unique_ptr<char[]>)>;
using output_channel = boost::asio::experimental::channel<void(boost::system::error_code, std::vector<char>)>;

}

template <typename T>
concept congestion_control_algorithm = requires(T algorithm) {
    true;
} && std::constructible_from<T, CongestionControl::output_channel&>;

class RenolikeCongestionControl {
public:
    RenolikeCongestionControl(CongestionControl::output_channel& output) noexcept
        : output_(output) {
    }

    ~RenolikeCongestionControl() {
        output_.close();
    }

    boost::asio::awaitable<void> Send(std::vector<char>&& message) {
        auto* messageBase = reinterpret_cast<MessageBase*>(message.data());
        messageBase->sequenceNumber = lastSentSequenceNumber;
        messageBase->streamId = streamId_;

        lastSentSequenceNumber += message.size();

        co_await output_.async_send(boost::system::error_code(), message, boost::asio::use_awaitable);
    }

    void SetStreamId(U16 streamId) {
        LOG_INFO("Set Stream ID to {}", streamId);
        streamId_ = streamId;
    }

    void PushMessage(std::vector<char> messageBuffer) {
        const auto* const message = reinterpret_cast<MessageBase*>(messageBuffer.data());

        if (message->sequenceNumber != ackNumber_) {
            LOG_WARNING("We received a message with sequence number {}, however we expected sequence number {}. Dropping the message and sending duplicate ACK.",
                        message->sequenceNumber, ackNumber_);

            std::vector<char> ackBuffer(sizeof(AckMessage));
            auto* ack = new(ackBuffer.data()) AckMessage{
                streamId_,
                MessageType::kAck,
                lastSentSequenceNumber,
                static_cast<U16>(receivedMessages_.capacity()),
                ackNumber_
            };

            output_.try_send(boost::system::error_code(), std::move(ackBuffer));

            lastSentSequenceNumber += sizeof(AckMessage);
            return;
        }

        if (message->messageType == MessageType::kAck) {
            LOG_TRACE("Acknowledged sequence number {}.", message->sequenceNumber);
            ackNumber_ += messageBuffer.size();
            return;
        }

        if (receivedMessages_.full()) {
            LOG_WARNING("Dropping a received message, since the buffer is full!");
            return;
        }

        //receivedMessages_.push_back()
        ackNumber_ += messageBuffer.size();

        std::vector<char> ackBuffer(sizeof(AckMessage));
        auto* ack = new(ackBuffer.data()) AckMessage{
            streamId_,
            MessageType::kAck,
            lastSentSequenceNumber,
            static_cast<U16>(receivedMessages_.capacity()),
            ackNumber_
        };

        output_.try_send(boost::system::error_code(), std::move(ackBuffer));

        lastSentSequenceNumber += sizeof(AckMessage);
    }

    boost::asio::awaitable<std::vector<char>> Receive() {
        while (receivedMessages_.empty()) {
            // Yes, busy waiting is bad. Sue me. 
            std::this_thread::yield();
        }

        auto&& message = receivedMessages_.front();
        receivedMessages_.pop_front();

        co_return message;
    }

private:
    using sequence_number = decltype(AckMessage::ackNumber);

    decltype(MessageBase::streamId) streamId_ = 0;
    CongestionControl::output_channel& output_;

    enum class State {
        kSlowStart,
        kCongestionAvoidance
    } state_ = State::kSlowStart;

    size_t congestionWindow_ = 1;
    size_t slowStartThreshold = 64;

    // The highest sequence number that was ACKed by us
    sequence_number ackNumber_ = 0;

    // The last sent sequence number
    sequence_number lastSentSequenceNumber = 0;

    // The last sequence number that was ACKed by the other endpoint
    sequence_number lastAcknowledged = 0;

    constexpr static size_t RECEIVE_WINDOW = 64;

    boost::circular_buffer<std::vector<char>> receivedMessages_{RECEIVE_WINDOW};
};

static_assert(congestion_control_algorithm<RenolikeCongestionControl>);

}

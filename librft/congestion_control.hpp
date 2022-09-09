#pragma once

#include <boost/circular_buffer.hpp>
#include <unordered_map>

#include "logger.hpp"

namespace rft {

namespace CongestionControl {

using input_channel = boost::asio::experimental::channel<void(boost::system::error_code, std::unique_ptr<char[]>)>;
using output_channel = boost::asio::experimental::channel<void(boost::system::error_code, std::unique_ptr<char[]>)>;

}

template <typename T>
concept congestion_control_algorithm = requires(T algorithm) {
    algorithm.Send(std::declval<std::unique_ptr<char[]>>());
    //{ co_await algorithm.Receive() } -> std::convertible_to<std::unique_ptr<char[]>>;
} && std::constructible_from<T, CongestionControl::input_channel&, CongestionControl::output_channel&>;

class RenolikeCongestionControl {
public:
    RenolikeCongestionControl(CongestionControl::input_channel& input, CongestionControl::output_channel& output) noexcept
        : output_(output) {
    }

    ~RenolikeCongestionControl() {
        output_.close();
    }

    boost::asio::awaitable<void> Send(std::unique_ptr<char[]> message) {

        co_await output_.async_send(boost::system::error_code(), std::move(message), boost::asio::use_awaitable); 
    }

    void ReceiveMessage(/*std::unique_ptr<char[]> messageBuffer*/ std::span<char> messageBuffer) {
        const auto* const message = reinterpret_cast<MessageBase*>(messageBuffer.data());

        if (message->sequenceNumber != ackNumber_) {
            LOG_WARNING("We received a message with sequence number {}, however we expected sequence number {}. Dropping the message and sending duplicate ACK.", message->sequenceNumber, ackNumber_);

            std::unique_ptr<char[]> ackBuffer(new char[sizeof(AckMessage)]);
            auto* ack = new(ackBuffer.get()) AckMessage {
                0, //FIXME: This needs to be the stream ID
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
            ackNumber_ += messageBuffer.size_bytes();
            return;
        }

        if (receivedMessages_.full()) {
            LOG_WARNING("Dropping a received message, since the buffer is full!");
            return;
        }

        //receivedMessages_.push_back()
        ackNumber_ += messageBuffer.size_bytes();

        std::unique_ptr<char[]> ackBuffer(new char[sizeof(AckMessage)]);
        auto* ack = new(ackBuffer.get()) AckMessage {
            0, //FIXME: This needs to be the stream ID
            MessageType::kAck,
            lastSentSequenceNumber,
            static_cast<U16>(receivedMessages_.capacity()),
            ackNumber_
        };

        output_.try_send(boost::system::error_code(), std::move(ackBuffer));

        lastSentSequenceNumber += sizeof(AckMessage);
    }

    boost::asio::awaitable<std::unique_ptr<char[]>> Receive() {
        while(receivedMessages_.empty()) {
            // Yes, busy waiting is bad. Sue me. 
            std::this_thread::yield();
        }

        auto&& message = receivedMessages_.front();
        receivedMessages_.pop_front();

        co_return std::move(message);
    }

private:
    using sequence_number = decltype(AckMessage::ackNumber);

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

    boost::circular_buffer<std::unique_ptr<char[]>> receivedMessages_{RECEIVE_WINDOW};

    std::unordered_map<sequence_number, std::tuple<bool, std::unique_ptr<char[]>>> sentMessages{};

    //boost::asio::steady_timer streamDeadTimer; 
};

static_assert(congestion_control_algorithm<RenolikeCongestionControl>);

}

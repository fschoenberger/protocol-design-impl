#pragma once

#include <boost/circular_buffer.hpp>

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
        : input_(input),
          output_(output) {
    }

    ~RenolikeCongestionControl() {
        //This is a bug, since we might double-close a channel when daisy-chaining them
        input_.close();
        output_.close();
    }

    boost::asio::awaitable<void> Send(std::unique_ptr<char[]> message) {
        auto* messageBase = reinterpret_cast<MessageBase*>(message.get());
        using namespace std::chrono_literals;

        if (ackNumber + windowSize <= messageBase->sequenceNumber) {
            LOG_WARNING("no ack received yet");
            std::this_thread::sleep_for(1s);
        }
        //LOG_INFO("ack number {}, sequence number {}", ackNumber, messageBase->sequenceNumber);
        if (messageBase->messageType == MessageType::kChunk || messageBase->messageType == MessageType::kServerHello) {
            // increasing sequence number
            LOG_INFO("increasing sequence number");
            messageBase->sequenceNumber++;
        }

        co_await output_.async_send(boost::system::error_code(), std::move(message), boost::asio::use_awaitable);
    }

    /* boost::asio::awaitable<std::unique_ptr<char[]>> Receive() {
        auto received = co_await input_.async_receive(boost::asio::use_awaitable);
        co_return std::move(received);
    }*/

    boost::asio::awaitable<std::unique_ptr<char[]>> TryGetMessage() {
        //using namespace std::chrono_literals;
        /* while (messages_.empty()) {
            std::this_thread::sleep_for(0s);
            PushMessage(std::move(received));
        }*/

        std::unique_ptr<char[]> message;
        if (!messages_.empty()) {
            message = std::move(messages_.front());
            messages_.pop_front();
        } else {
            message = co_await input_.async_receive(boost::asio::use_awaitable);
        }

        auto* messageBase = reinterpret_cast<MessageBase*>(message.get());
        //LOG_INFO("ack number {}, sequence number {}", ackNumber, messageBase->sequenceNumber);
        // Send Acknowledgement
        if (messageBase->sequenceNumber - windowSize < ackNumber && messageBase->messageType != MessageType::kAck) {
            ackNumber = messageBase->sequenceNumber;
            auto* ackMessage = new AckMessage{messageBase->streamId, MessageType::kAck, messageBase->sequenceNumber, windowSize, ackNumber};
            std::unique_ptr<char[]> message{reinterpret_cast<char*>(ackMessage)};
            co_await Send(std::move(message));
        }

        co_return std::move(message);
    }

    bool PushMessage(std::unique_ptr<char[]> message) {
        auto* messageBase = reinterpret_cast<MessageBase*>(message.get());
        if (messageBase->messageType == MessageType::kAck) {
            // update the current ackNumber
            auto* ackMessage = reinterpret_cast<AckMessage*>(message.get());
            ackNumber = ackMessage->ackNumber;
        }
        // add message to buffer:
        messages_.push_back(std::move(message));
        return true;
    }

private:
    CongestionControl::input_channel& input_;
    CongestionControl::output_channel& output_;
    U64 ackNumber = 0;
    U16 windowSize = 100; //TODO what starting window size too choose ?

    boost::circular_buffer<std::unique_ptr<char[]>> messages_;
};

static_assert(congestion_control_algorithm<RenolikeCongestionControl>);

}

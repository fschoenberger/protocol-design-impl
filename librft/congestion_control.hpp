#pragma once

#include <boost/circular_buffer.hpp>

#include "logger.hpp"

namespace rft {

namespace CongestionControl {

using input_channel = boost::asio::experimental::channel<void(boost::system::error_code, void*)>;
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
        LOG_INFO("Hello from send!");

        co_await output_.async_send(boost::system::error_code(), std::move(message), boost::asio::use_awaitable);

        co_return;
    }

    boost::asio::awaitable<std::unique_ptr<char[]>> Receive() {
        co_await input_.async_receive(boost::asio::use_awaitable);

        co_return nullptr;
    }

private:
    CongestionControl::input_channel& input_;
    CongestionControl::output_channel& output_;
};

static_assert(congestion_control_algorithm<RenolikeCongestionControl>);

}

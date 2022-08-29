#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/circular_buffer.hpp>
#include <chrono>
#include <tuple>

#include <hash-library/sha256.h>

#include "logger.hpp"
#include "messages.hpp"
#include "congestion_control.hpp"

namespace rft {

template <congestion_control_algorithm C>
class ClientStream;

class Client {
    // TODO: This is a code smell
    template <congestion_control_algorithm C>
    friend class ClientStream;

public:
    explicit Client(boost::asio::any_io_executor executor);

    boost::asio::awaitable<void> Run();

private:
    constexpr static auto MAX_LENGTH = 1024 - 8;

    boost::asio::ip::udp::socket socket_;
    boost::asio::any_io_executor executor_;

    std::map<decltype(MessageBase::streamId), ClientStream<RenolikeCongestionControl>> streams_{};
    std::map<decltype(MessageBase::streamId), std::tuple<CongestionControl::input_channel, CongestionControl::output_channel>> channels_{};
};


template <congestion_control_algorithm CongestionControlMixin>
class ClientStream : private CongestionControlMixin {
public:
    ClientStream(
        boost::asio::any_io_executor executor,
        CongestionControl::input_channel& inputChannel,
        CongestionControl::output_channel& outputChannel,
        U16 streamId,
        const ClientHello* const message)
        : CongestionControlMixin(inputChannel, outputChannel),
          id_(streamId),
          file_(executor),
          executor_(executor) {

        //This might be a bug,when the string is not 0-terminated? Maybe?
        std::string filename{message->fileName};
        file_.open(filename, boost::asio::file_base::read_only);
        LOG_INFO("New stream {} for file {} established.", streamId, filename);
    }

    ~ClientStream() {
        LOG_INFO("Cleaned up stream {}.", id_);
    }

    boost::asio::awaitable<void> SendClientHello() {
        co_return;
    }

    boost::asio::awaitable<void> Run() {
        using namespace std::chrono_literals;
        using namespace boost::asio::experimental::awaitable_operators;

        try {
            co_await SendClientHello();
        } catch (const std::exception& e) {
            LOG_ERROR("Stream {}: There was an excpetion {}.", id_, e.what());
        }

        co_return;
    }

private:
    using CongestionControlMixin::Send;
    using CongestionControlMixin::Receive;

    static constexpr const size_t MAX_BUFFER_SIZE = 15;

    const decltype(MessageBase::streamId) id_;
    boost::asio::random_access_file file_;
    boost::asio::any_io_executor executor_;
};

}
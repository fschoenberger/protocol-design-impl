#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool.hpp>
#include <random>
#include <chrono>
#include <tuple>

#include "logger.hpp"
#include "stream.hpp"
#include "congestion_control.hpp"

namespace rft {

template <congestion_control_algorithm C>
class ServerStream;

class Server {
    // TODO: This is a code smell
    template <congestion_control_algorithm C>
    friend class ServerStream;

public:
    Server(boost::asio::any_io_executor executor, short serverPort);

    boost::asio::awaitable<void> Run();

private:
    constexpr static auto MAX_LENGTH = 1024 - 8;

    boost::asio::ip::udp::socket socket_;
    boost::asio::any_io_executor executor_;

    std::map<decltype(MessageBase::streamId), ServerStream<RenolikeCongestionControl>> streams_{};
    std::map<decltype(MessageBase::streamId), std::tuple<CongestionControl::input_channel, CongestionControl::output_channel>> channels_{};

    static std::random_device random;
    static std::uniform_int<std::uint16_t> distribution;
};


template <congestion_control_algorithm CongestionControlMixin>
class ServerStream : private CongestionControlMixin {
public:
    ServerStream(
        boost::asio::any_io_executor executor,
        CongestionControl::input_channel& inputChannel,
        CongestionControl::output_channel& outputChannel,
        U16 streamId,
        const ClientHello* const message)
        : CongestionControlMixin(inputChannel, outputChannel),
          id_(streamId),
          file_(executor, "C:\\source\\boost-pool\\index.html", boost::asio::file_base::read_only),
          executor_(executor) {
        LOG_INFO("New stream {} established.", streamId);
    }

    ~ServerStream() {
        LOG_INFO("Cleaned up stream {}.", id_);
    }

    boost::asio::awaitable<void> SendServerHello() {
        //TODO: Calculate hash sum

        std::unique_ptr<char[]> message{reinterpret_cast<char*>(new ServerHello{
            id_,
            MessageType::kServerHello,
            0,
            0,
            0,
            0,
            0,
            {0},
            0,
            file_.size()
        })};

        co_await Send(std::move(message));
    }

    boost::asio::awaitable<void> Run() {
        using namespace std::chrono_literals;
        using namespace boost::asio::experimental::awaitable_operators;

        try {
            co_await SendServerHello();

            {
                // Await first client ack
                boost::asio::steady_timer t(executor_, 5s);
                const auto result = co_await (t.async_wait(boost::asio::use_awaitable) || Receive());

                if (result.index() == 0) {
                    LOG_INFO("5 seconds expired and we got no client ACK. Destroying connection.");
                    co_return;
                }
            }

            auto firstChunk = 3;
            auto lastChunk = 12;
            constexpr auto CHUNK_SIZE = 1024;

            for (int i = firstChunk; i < lastChunk; ++i) {
                std::array<char, CHUNK_SIZE> data{0};

                // TODO: Check if function supports EOF
                // auto bytesRead = co_await async_read_at(file_, i * CHUNK_SIZE, boost::asio::buffer(data), boost::asio::use_awaitable);

                co_await Send(nullptr);
            }

            {
                // Fin Message
                /*FinMessage message{};
                co_await Send(&message);*/
            }
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

    //CongestionControl::input_channel& inputChannel_;
    //CongestionControl::output_channel& outputChannel_;

    std::uint16_t clientWindowInMessages_;
    std::uint16_t sendWindow_ = 1;
};

} // namespace rft

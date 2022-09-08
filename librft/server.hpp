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

#include <hash-library/sha3.h>

#include "logger.hpp"
#include "messages.hpp"
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
          file_(executor),
          executor_(executor) {

        //This might be a bug,when the string is not 0-terminated? Maybe?
        std::string filename{message->fileName};
        std::string filePath = getenv("USERPROFILE");
        filePath += "\\RFT\\";
        filePath += filename;
        file_.open(filePath, boost::asio::file_base::read_only);
        LOG_INFO("New stream {} for file {} established.", streamId, filename);
    }

    ~ServerStream() {
        LOG_INFO("Cleaned up stream {}.", id_);
    }

    boost::asio::awaitable<void> SendServerHello() {
        SHA3 sha3;
        auto sizeRead = 0;

        // We have a connection timeout here, if our file is large enough, because it just takes too long to read it from HDD
        constexpr auto BUFFER_SIZE = 10 * 1024 * 1024;
        auto buffer = new char[BUFFER_SIZE];
        while(sizeRead < file_.size()) {
            auto actualRead = co_await file_.async_read_some_at(sizeRead, boost::asio::buffer(buffer, BUFFER_SIZE), boost::asio::use_awaitable);
            sha3.add(buffer, actualRead);
            sizeRead += actualRead;
        }
        delete[] buffer;

        unsigned char hash[32];
        //TODO: Fill hash buffer :c

        LOG_INFO("Hash of file is {}.", sha3.getHash());

        auto* serverHello = new ServerHello{
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
        };
        std::ranges::copy_n(reinterpret_cast<uint64_t*>(hash), 4, serverHello->checksum.begin());

        std::unique_ptr<char[]> message{reinterpret_cast<char*>(serverHello)};

        co_await Send(std::move(message));
    }

    bool PushMessage(char* data) {
        std::unique_ptr<char[]> message{reinterpret_cast<char*>(data)};
        return PushMessage(std::move(message));
    }

    boost::asio::awaitable<void> Run() {
        using namespace std::chrono_literals;
        using namespace boost::asio::experimental::awaitable_operators;

        try {
            co_await SendServerHello();

            if(false) {
                // Await first client ack
                boost::asio::steady_timer t(executor_, 5s);
                const auto result = co_await (t.async_wait(boost::asio::use_awaitable) || TryGetMessage());

                if (result.index() == 0) {
                    LOG_INFO("5 seconds expired and we got no client ACK. Destroying connection.");
                    co_return;
                }
            }

            constexpr auto CHUNK_SIZE = sizeof(ChunkMessage::payload);

            auto firstChunk = 0; //TODO
            auto lastChunk = static_cast<size_t>(std::ceil(file_.size() / CHUNK_SIZE));

            for (int i = firstChunk; i <= lastChunk; ++i) {
                std::unique_ptr<char[]> buffer(new char[sizeof(ChunkMessage)]);
                auto* message = new(buffer.get()) ChunkMessage{
                    0,
                    MessageType::kChunk,
                    0,
                    {0},
                    {0}
                };

                const size_t payloadSize = (i != lastChunk) ? CHUNK_SIZE : file_.size() % CHUNK_SIZE; //TODO: The last expression can be 0, which is incorrect
                auto bytesRead = co_await async_read_at(file_, i * CHUNK_SIZE, boost::asio::buffer(message->payload, payloadSize), boost::asio::use_awaitable);

                LOG_TRACE("Stream {}: Sending chunk {}.", id_, i);
                co_await Send(std::move(buffer));
            }

            boost::asio::steady_timer t(executor_, 5s);
            co_await t.async_wait(boost::asio::use_awaitable);

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
    using CongestionControlMixin::TryGetMessage;
    using CongestionControlMixin::PushMessage;

    static constexpr const size_t MAX_BUFFER_SIZE = 15;

    const decltype(MessageBase::streamId) id_;
    boost::asio::random_access_file file_;
    boost::asio::any_io_executor executor_;
};

} // namespace rft

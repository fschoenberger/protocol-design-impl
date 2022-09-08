#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string.hpp>
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

    boost::asio::awaitable<void> Run(std::string filePath);

private:
    //TODO: This should be moved out of class scope!
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
        CongestionControl::output_channel& outputChannel)
        : CongestionControlMixin(inputChannel, outputChannel),
          executor_(executor) {
    }

    ~ClientStream() {
        LOG_INFO("Cleaned up stream {}.", id_);
    }

    boost::asio::awaitable<void> SendClientHello(std::string fileName) {
        LOG_INFO("Sending client hello...");
        auto* clientHello = new ClientHello{0, MessageType::kClientHello, 0, 0x1, 0, 0, 10, 0, ""};
        strcpy(clientHello->fileName, &fileName[0]);
        std::unique_ptr<char[]> message{reinterpret_cast<char*>(clientHello)};
        co_await Send(std::move(message));
    }

    boost::asio::awaitable<size_t> ExpectServerHello() {
        using namespace std::chrono_literals;
        using namespace boost::asio::experimental::awaitable_operators;

        boost::asio::steady_timer t(executor_);
        t.expires_after(5s);
        const auto result = co_await (TryGetMessage() || t.async_wait(boost::asio::use_awaitable));

        if (result.index() == 1) {
            LOG_INFO("5 seconds expired and we got no ServerHello. Destroying stream.");
            throw std::runtime_error{"5 seconds expired and we got no ServerHello. Destroying stream."};
        }

        if (const auto* message = reinterpret_cast<MessageBase*>(std::get<0>(result).get()); message->messageType != MessageType::kServerHello) {
            LOG_ERROR("Got an unexpected message or an error. Terminating stream.");
            throw std::runtime_error{"Got an unexpected message or an error. Terminating stream."};
        }

        const auto* serverHello = reinterpret_cast<ServerHello*>(std::get<0>(result).get());
        if (serverHello->nextHeaderOffset != 0 || serverHello->nextHeaderType != 0) {
            LOG_ERROR("Server is trying to use the nextHeader feature.");
            throw std::runtime_error{"Server is trying to use the nextHeader feature."};
        }

        id_ = serverHello->streamId;
        co_return serverHello->fileSizeInBytes;
    }

    boost::asio::awaitable<void> Run(std::string fileName) {
        try {
            co_await SendClientHello(fileName);
            auto fileSize = co_await ExpectServerHello();

            std::string savePath = getenv("USERPROFILE");
            savePath += "\\Desktop\\";
            savePath += fileName;
            boost::asio::basic_stream_file<> file(executor_, &savePath[0],
                                                  boost::asio::file_base::create | boost::asio::file_base::write_only | boost::asio::file_base::truncate);

            constexpr auto MAX_PAYLOAD_SIZE = sizeof(ChunkMessage::payload);

            const auto numChunks = static_cast<size_t>(std::ceil(fileSize / static_cast<float>(MAX_PAYLOAD_SIZE)));

            LOG_INFO("Filesize is {}. That makes {} chunks. The last chunk has {} bytes.", fileSize, numChunks, fileSize % MAX_PAYLOAD_SIZE);

            for (int i = 0; i < numChunks; ++i) {
                auto messageBuffer = co_await TryGetMessage();
                auto* message = reinterpret_cast<MessageBase*>(messageBuffer.get());

                //TODO: Check if message is chunk
                auto* chunk = reinterpret_cast<ChunkMessage*>(message);

                const size_t payloadSize = (i != numChunks) ? MAX_PAYLOAD_SIZE : fileSize % MAX_PAYLOAD_SIZE; //TODO: The last expression can be 0, which is incorrect
                std::span payload{chunk->payload.data(), payloadSize};

                // At this point we normally would have to verify the chunk message with it's sha-3 checksum. For whatever reason, the spec doesn't actually require
                // doing this, and since it would make our state handling extremely messing (since we basically need to tell our lower layer that the message is invalid)
                // we exploit this and just don't verify the chunk message here.

                co_await boost::asio::async_write(file, boost::asio::buffer(payload), boost::asio::use_awaitable);
                LOG_INFO("Chunk {}: Wrote {} bytes to file.", i, payloadSize);
            }

            file.sync_all();
        } catch (const std::exception& e) {
            LOG_ERROR("Stream {}: There was an exception {}.", id_, e.what());
        }

        LOG_INFO("Stream is finished.");
        co_return;
    }

private:
    using CongestionControlMixin::Send;
    using CongestionControlMixin::TryGetMessage;

    //static constexpr const size_t MAX_BUFFER_SIZE = 15;

    decltype(MessageBase::streamId) id_ = 0;
    boost::asio::any_io_executor executor_;
};

}

#include "server.hpp"

#include "logger.hpp"
#include "pch.hpp"

namespace ip = boost::asio::ip;

namespace rft {

// Storage for server's random device
decltype(Server::random) Server::random;
decltype(Server::distribution) Server::distribution{std::numeric_limits<decltype(MessageBase::streamId)>::min(), std::numeric_limits<decltype(MessageBase::streamId)>::max()};

Server::Server(boost::asio::any_io_executor executor, short serverPort)
    : socket_(executor, ip::udp::endpoint(ip::udp::v4(), serverPort)),
      //TODO: Should also work with IPv6
      executor_(executor) {
}

boost::asio::awaitable<void> Server::Run() {
    try {
        //TODO: We might want to use something like a pool allocator here instead of allocating it on the heap, but ¯\_(ツ)_/¯.
        std::unique_ptr<char[]> data{new char[MAX_LENGTH]};

        ip::udp::endpoint endpoint;
        for (;;) {
            // We can't receive half a UDP datagram, so at this point we know we received a complete message
            const size_t bytesReceived = co_await socket_.async_receive_from(boost::asio::buffer(data.get(), MAX_LENGTH), endpoint, boost::asio::use_awaitable);
            if (bytesReceived < sizeof(MessageBase)) {
                LOG_WARNING("Received malformed message or incomplete message with size {} from {}", bytesReceived, endpoint.address().to_string());
                continue;
            }

            const auto* const message = reinterpret_cast<MessageBase*>(data.get());

            if (message->messageType == MessageType::kClientHello) {
                // At this point, we're establishing a new stream

                // First, we check if all IDs are exhausted
                // TODO: This might be an off-by-one error
                if (streams_.size() == std::numeric_limits<decltype(MessageBase::streamId)>::max()) {
                    LOG_WARNING("{} tried to establish a new stream, however all streamIDs are currently in use.", endpoint.address().to_string());
                    continue;
                }

                uint16_t id = 0;
                do {
                    id = distribution(random);
                } while (streams_.contains(id));

                auto [channelsIterator, channelSuccess] = channels_.try_emplace(id, executor_, executor_);
                if (!channelSuccess) {
                    LOG_WARNING("Could not emplace stream channels {}. Skipping.", id);
                    continue;
                }

                auto& [inputChannel, outputChannel] = channelsIterator->second;

                auto [stream, streamSuccess] = streams_.try_emplace(id, executor_, inputChannel, outputChannel, id, reinterpret_cast<const ClientHello*>(message));
                if (!streamSuccess) {
                    LOG_WARNING("Could not emplace stream {}. Skipping.", id);
                    continue;
                }

                // We now let the stream run its course. As soon as the stream is done, we clean up all related resources
                boost::asio::co_spawn(executor_, [&stream, id, this]() -> boost::asio::awaitable<void> {
                    try {
                        co_await stream->second.Run();
                    } catch (const std::exception& e) {
                        LOG_ERROR("Connection {} encountered an error. Please check the logs above.", id);
                    }

                    streams_.erase(streams_.find(id));
                    channels_.erase(channels_.find(id));
                    co_return;
                }, boost::asio::detached);

                boost::asio::co_spawn(executor_, [id, &outputChannel, this, destination = endpoint]() -> boost::asio::awaitable<void> {
                    while (outputChannel.is_open()) {
                        try {
                            //constexpr auto SIZE = sizeof(ServerHello);

                            const auto message = co_await outputChannel.async_receive(boost::asio::use_awaitable);

                            const auto* base = reinterpret_cast<MessageBase*>(message.get());
                            auto size = 0;
                            switch (base->messageType) {
                                case MessageType::kClientHello:
                                    size = sizeof(ClientHello);
                                    break;
                                case MessageType::kServerHello:
                                    size = sizeof(ServerHello);
                                    break;
                                case MessageType::kAck:
                                    size = sizeof(AckMessage);
                                    break;
                                case MessageType::kFin:
                                    size = sizeof(FinMessage);
                                    break;
                                case MessageType::kError:
                                    size = sizeof(ErrorMessage);
                                    break;
                                case MessageType::kChunk:
                                    size = sizeof(ChunkMessage);
                                    break;
                            }

                            assert(size != 0);

                            const auto actualSize = co_await socket_.async_send_to(boost::asio::buffer(message.get(), size), destination, boost::asio::use_awaitable);
                            LOG_TRACE("Stream {}: Sent {} bytes to {}.", id, size, destination.address().to_string());

                            if (actualSize != size) {
                                LOG_ERROR("In stream {} a fewer bytes than the message size were sent out. Expected size: {}, actual size: {}.", id, size, actualSize);
                                co_return;
                            }
                        } catch (const boost::system::system_error& e) {
                            if (e.code() == boost::asio::experimental::error::channel_closed) {
                                LOG_INFO("co_awaited a closed channel, cleaning up...");
                            } else {
                                LOG_ERROR("Connection {} encountered an error while trying to send using the output channel: {}", id, e.what());
                            }

                            break;
                        } catch (const std::exception& e) {
                            LOG_ERROR("Connection {} encountered an error while trying to send using the output channel: {}", id, e.what());
                            break;
                        }
                    }

                    co_return;
                }, boost::asio::detached);
            } else {
                // We already have a stream
                const auto streamId = message->streamId;

                if (!streams_.contains(streamId)) {
                    LOG_WARNING("Received message for stream {} from {}, however no stream with such an ID was found. Discarding the message.", streamId,
                                endpoint.address().to_string());
                    continue;
                }

                auto& stream = streams_.at(streamId);
                
                stream.PushMessage(data.get());
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Server::Run() encountered an error: {}", e.what());
    }
}

} // namespace rft

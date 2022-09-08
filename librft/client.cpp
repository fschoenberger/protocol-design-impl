#include "pch.hpp"
#include "client.hpp"

namespace ip = boost::asio::ip;

namespace rft {

Client::Client(boost::asio::any_io_executor executor)
    : socket_(executor, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
      executor_(executor) {

}

boost::asio::awaitable<void> Client::Run(std::string fileName) {
    using namespace boost::asio::experimental::awaitable_operators;

    CongestionControl::input_channel inputChannel(executor_);
    CongestionControl::output_channel outputChannel(executor_);

    ClientStream<RenolikeCongestionControl> clientStream(executor_, inputChannel, outputChannel);

    auto sender = [&outputChannel, this]() -> boost::asio::awaitable<void> {
        auto id = -1;

        while (outputChannel.is_open()) {
            try {
                constexpr auto SIZE = sizeof(ClientHello);

                const auto message = co_await outputChannel.async_receive(boost::asio::use_awaitable);

                const auto size = co_await socket_.async_send_to(boost::asio::buffer(message.get(), SIZE), ip::udp::endpoint(ip::address::from_string("127.0.0.2"), 5051),
                                                                 boost::asio::use_awaitable);
                LOG_TRACE("Stream {}: Sent {} bytes.", id, size);

                if (size != SIZE) {
                    LOG_ERROR("In stream {} a fewer bytes than the message size were sent out. Expected size: {}, actual size: {}.", id, SIZE, size);
                    co_return;
                }
            } catch (const boost::system::system_error& e) {
                if (e.code() == boost::asio::experimental::error::channel_closed) {
                    LOG_INFO("co_awaited a closed channel, cleaning up...");
                } else {
                    LOG_ERROR("Stream {} encountered an error while trying to send using the output channel: {}", id, e.what());
                }

                break;
            } catch (const std::exception& e) {
                LOG_ERROR("Stream {} encountered an error while trying to send using the output channel: {}", id, e.what());
                break;
            }
        }

        co_return;
    };

    auto receiver = [&inputChannel, this]() -> boost::asio::awaitable<void> {
        try {
            while (inputChannel.is_open()) {
                std::unique_ptr<char[]> data(new char[MAX_LENGTH]);

                ip::udp::endpoint endpoint;

                auto size = co_await socket_.async_receive_from(boost::asio::buffer(data.get(), MAX_LENGTH), endpoint, boost::asio::use_awaitable);
                LOG_TRACE("Received {} bytes from {}.", size, endpoint.address().to_string());

                co_await inputChannel.async_send(boost::system::error_code(), std::move(data), boost::asio::use_awaitable);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Receiving failed because {}.", e.what());
        }
    };

    // If one of the coroutines end, the others are cancelled as well
    co_await (receiver() || sender() || clientStream.Run(fileName));

    LOG_INFO("Exiting Client::Run()");
    co_return;
}

}

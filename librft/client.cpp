#include "pch.hpp"
#include "client.hpp"

namespace ip = boost::asio::ip;

namespace rft {

Client::Client(boost::asio::any_io_executor executor)
    : socket_(executor, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
      executor_(executor) {

}

boost::asio::awaitable<void> Client::Run() {
    using namespace boost::asio::experimental::awaitable_operators;

    CongestionControl::output_channel outputChannel(executor_);

    ClientStream<RenolikeCongestionControl> clientStream(executor_,  outputChannel);

    auto sender = [&outputChannel, this]() -> boost::asio::awaitable<void> {
        while (outputChannel.is_open()) {
            try {
                const auto message = co_await outputChannel.async_receive(boost::asio::use_awaitable);

                const auto size = co_await socket_.async_send_to(boost::asio::buffer(message), ip::udp::endpoint(ip::address::from_string("127.0.0.2"), 5051),
                                                                 boost::asio::use_awaitable);
                LOG_TRACE("Sent {} bytes.", size);

                if (size != message.size()) {
                    LOG_ERROR("Fewer bytes than the message size were sent out. Expected size: {}, actual size: {}.", message.size(), size);
                    co_return;
                }
            } catch (const boost::system::system_error& e) {
                if (e.code() == boost::asio::experimental::error::channel_closed) {
                    LOG_INFO("co_awaited a closed channel, cleaning up...");
                } else {
                    LOG_ERROR("We encountered an error while trying to send using the output channel: {}", e.what());
                }

                break;
            } catch (const std::exception& e) {
                LOG_ERROR("We encountered an error while trying to send using the output channel: {}", e.what());
                break;
            }
        }

        co_return;
    };

    auto receiver = [&clientStream, this]() -> boost::asio::awaitable<void> {
        try {
            for(;;) {
                std::vector<char> data(MAX_LENGTH);

                ip::udp::endpoint endpoint;

                auto size = co_await socket_.async_receive_from(boost::asio::buffer(data), endpoint, boost::asio::use_awaitable);
                data.resize(size);

                LOG_TRACE("Received {} ({}) bytes from {}.", size, data.size(), endpoint.address().to_string());

                clientStream.PushMessage(std::move(data));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Receiving failed because {}.", e.what());
        }
    };

    // If one of the coroutines end, the others are cancelled as well
    co_await (receiver() || sender() || clientStream.Run());

    LOG_INFO("Exiting Client::Run()");
    co_return;
}

}

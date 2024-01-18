// based on:
// https://www.boost.org/doc/libs/1_84_0/libs/beast/example/websocket/server/async/websocket_server_async.cpp

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>

using namespace boost;

void fail(beast::error_code ec, char const *what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

class Session : public std::enable_shared_from_this<Session> {
public:
  explicit Session(asio::ip::tcp::socket &&socket)
      : socket(std::move(socket)) {}

  void run() {
    asio::dispatch(
        socket.get_executor(),
        beast::bind_front_handler(&Session::on_run, shared_from_this()));
  }

  void on_run() {
    // Set suggested timeouts. May change to stay alive forever?
    socket.set_option(beast::websocket::stream_base::timeout::suggested(
        beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    socket.set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::response_type &res) {
          res.set(beast::http::field::server,
                  std::string(BOOST_BEAST_VERSION_STRING) +
                      " MillenniumDB Server");
        }));

    // Accept the websocket handshake
    socket.async_accept(
        beast::bind_front_handler(&Session::on_accept, shared_from_this()));
  }

  void on_accept(beast::error_code ec) {
    if (ec)
      return fail(ec, "accept");

    do_read();
  }

  void do_read() {
    socket.async_read(buffer, beast::bind_front_handler(&Session::on_read,
                                                        shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == beast::websocket::error::closed)
      return;

    if (ec)
      return fail(ec, "read");

    socket.text(socket.got_text());
    socket.async_write(
        buffer.data(),
        beast::bind_front_handler(&Session::on_write, shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec)
      return fail(ec, "write");

    buffer.consume(buffer.size());

    do_read();
  }

private:
  beast::websocket::stream<beast::tcp_stream> socket;
  beast::flat_buffer buffer;
};

class Listener : public std::enable_shared_from_this<Listener> {
public:
  Listener(asio::io_context &io_context_, asio::ip::tcp::endpoint endpoint)
      : io_context(io_context_), acceptor(io_context) {
    boost::system::error_code ec;
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
      fail(ec, "open");
      return;
    }
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
      fail(ec, "set_option");
      return;
    }
    acceptor.bind(endpoint, ec);
    if (ec) {
      fail(ec, "bind");
      return;
    }
    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
      fail(ec, "listen");
      return;
    }
  }

  void run() { do_accept(); }

private:
  asio::io_context &io_context;
  asio::ip::tcp::acceptor acceptor;

  void do_accept() {
    acceptor.async_accept(
        asio::make_strand(io_context),
        beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
  };

  void on_accept(beast::error_code ec, asio::ip::tcp::socket socket) {
    if (ec) {
      fail(ec, "accept");
    } else {
      std::make_shared<Session>(std::move(socket))->run();
    }
    do_accept();
  }
};

int main() {
  auto const port = 3001;
  auto const num_threads = 1;

  asio::io_context io_context{num_threads};

  std::make_shared<Listener>(io_context,
                             asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
      ->run();

  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < num_threads - 1; ++i) {
    threads.emplace_back([&io_context] { io_context.run(); });
  }

  std::cout << "WebSocket server listening on port " << port << "...\n";

  io_context.run();

  return EXIT_SUCCESS;
}
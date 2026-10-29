/*

dialog.cpp
----------

Copyright (C) 2016, Tomislav Karastojkovic (http://www.alepho.com).

Distributed under the FreeBSD license, see the accompanying file LICENSE or
copy at http://www.freebsd.org/copyright/freebsd-license.html.

*/


#include <string>
#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <mailio/dialog.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <openssl/ssl.h>


using std::string;
using std::to_string;
using std::move;
using std::istream;
using std::make_shared;
using std::shared_ptr;
using std::bind;
using std::chrono::milliseconds;
using boost::asio::ip::tcp;
using boost::asio::buffer;
using boost::asio::streambuf;
using boost::asio::io_context;
using boost::asio::steady_timer;
using boost::asio::ssl::context;
using boost::system::system_error;
using boost::system::error_code;
using boost::algorithm::trim_if;
using boost::algorithm::is_any_of;


namespace mailio
{


dialog::dialog(const string& hostname, unsigned port, milliseconds timeout) : std::enable_shared_from_this<dialog>(),
    hostname_(hostname), port_(port), ios_(make_shared<io_context>()), resolver_(make_shared<tcp::resolver>(*ios_)),
    socket_(make_shared<tcp::socket>(*ios_)), timer_(make_shared<steady_timer>(*ios_)),
    timeout_(timeout), timer_expired_(false), strmbuf_(make_shared<streambuf>(32 * 1024 * 1024)), istrm_(make_shared<istream>(strmbuf_.get()))
{
}


dialog::dialog(const dialog& other) : std::enable_shared_from_this<dialog>(),
    hostname_(other.hostname_), port_(other.port_), ios_(other.ios_), resolver_(other.resolver_), socket_(other.socket_), timer_(other.timer_),
    timeout_(other.timeout_), timer_expired_(other.timer_expired_), strmbuf_(other.strmbuf_), istrm_(other.istrm_)
{
}


void dialog::connect()
{
    try
    {
        if (timeout_.count() == 0)
        {
            tcp::resolver res(*ios_);
            boost::asio::connect(*socket_, res.resolve(hostname_, to_string(port_)));
        }
        else
            connect_async();
    }
    catch (const system_error& exc)
    {
        throw dialog_error("Server connecting failed.", exc.code().message());
    }
}


void dialog::send(const string& line)
{
    if (timeout_.count() == 0)
        send_sync(*socket_, line);
    else
        send_async(*socket_, line);
}


// TODO: perhaps the implementation should be common with `receive_raw()`
string dialog::receive(bool raw)
{
    if (timeout_.count() == 0)
        return receive_sync(*socket_, raw);
    else
        return receive_async(*socket_, raw);
}


template<typename Socket>
void dialog::send_sync(Socket& socket, const string& line)
{
    try
    {
        string l = line + "\r\n";
        write(socket, buffer(l, l.size()));
    }
    catch (const system_error& exc)
    {
        throw dialog_error("Network sending error.", exc.code().message());
    }
}


template<typename Socket>
string dialog::receive_sync(Socket& socket, bool raw)
{
    try
    {
        read_until(socket, *strmbuf_, "\n");
        string line;
        getline(*istrm_, line, '\n');
        if (!raw)
            trim_if(line, is_any_of("\r\n"));
        return line;
    }
    catch (const system_error& exc)
    {
        throw dialog_error("Network receiving error.", exc.code().message());
    }
}


void dialog::connect_async()
{
    check_timeout();
    bool has_connected{false}, connect_error{false};
    error_code errc;
    resolver_->async_resolve(hostname_, to_string(port_),
        [this, &has_connected, &connect_error, &errc](const error_code& error, tcp::resolver::results_type endpoints)
        {
            if (error) { connect_error = true; errc = error; return; }
            async_connect(*socket_, endpoints,
                [&has_connected, &connect_error, &errc](const error_code& ec, const tcp::endpoint&)
                { has_connected = !ec; connect_error = !!ec; errc = ec; });
        });
    wait_async(has_connected, connect_error, "Network connecting timed out.", "Network connecting failed.", errc);
}


template<typename Socket>
void dialog::send_async(Socket& socket, string line)
{
    check_timeout();
    string l = line + "\r\n";
    bool has_written{false}, send_error{false};
    error_code errc;
    async_write(socket, buffer(l, l.size()),
        [&has_written, &send_error, &errc](const error_code& error, size_t)
        {
            if (!error)
                has_written = true;
            else
                send_error = true;
            errc = error;
        });
    wait_async(has_written, send_error, "Network sending timed out.", "Network sending failed.", errc);
}


template<typename Socket>
string dialog::receive_async(Socket& socket, bool raw)
{
    check_timeout();
    bool has_read{false}, receive_error{false};
    string line;
    error_code errc;
    async_read_until(socket, *strmbuf_, "\n",
        [&has_read, &receive_error, this, &line, &errc, raw](const error_code& error, size_t)
        {
            if (!error)
            {
                getline(*istrm_, line, '\n');
                if (!raw)
                    trim_if(line, is_any_of("\r\n"));
                has_read = true;
            }
            else
                receive_error = true;
            errc = error;
        });
    wait_async(has_read, receive_error, "Network receiving timed out.", "Network receiving failed.", errc);
    return line;
}


void dialog::wait_async(const bool& has_op, const bool& op_error, const char* expired_msg, const char* op_msg, const error_code& error)
{
    while (!has_op && !op_error)
        ios_->run_one();
    timer_->cancel();
    // Complete cancellation callbacks before any stack-captured state disappears.
    ios_->poll();
    if (timer_expired_)
        throw dialog_error(expired_msg, error.message());
    if (op_error)
        throw dialog_error(op_msg, error.message());
}


void dialog::check_timeout()
{
    ios_->restart();
    // Expiring automatically cancels the timer, per documentation.
    timer_->expires_after(timeout_);
    timer_expired_ = false;
    timer_->async_wait([this](const error_code& error) { timeout_handler(error); });
}


void dialog::timeout_handler(const error_code& error)
{
    if (!error)
    {
        timer_expired_ = true;
        resolver_->cancel();
        error_code ignored;
        socket_->close(ignored);
    }
}


dialog_ssl::dialog_ssl(const string& hostname, unsigned port, milliseconds timeout, const ssl_options_t& options) :
    dialog(hostname, port, timeout), ssl_(false), context_(make_shared<context>(options.method)),
    ssl_socket_(make_shared<boost::asio::ssl::stream<tcp::socket&>>(*socket_, *context_))
{
}


dialog_ssl::dialog_ssl(const dialog& other, const ssl_options_t& options) : dialog(other), ssl_(false), context_(make_shared<context>(options.method)),
    ssl_socket_(make_shared<boost::asio::ssl::stream<tcp::socket&>>(*socket_, *context_))
{
    try
    {
        context_->set_default_verify_paths();
        if (!options.ca_file.empty())
            context_->load_verify_file(options.ca_file);
        // TLS is fail-closed even if an old caller supplies verify_none.
        ssl_socket_->set_verify_mode(boost::asio::ssl::verify_peer);
        ssl_socket_->set_verify_callback(boost::asio::ssl::host_name_verification(hostname_));
        error_code address_error;
        boost::asio::ip::make_address(hostname_, address_error);
        if (address_error && !SSL_set_tlsext_host_name(ssl_socket_->native_handle(), hostname_.c_str()))
            throw dialog_error("Setting TLS server name failed.", "");
        if (SSL_set_min_proto_version(ssl_socket_->native_handle(), TLS1_2_VERSION) != 1)
            throw dialog_error("Setting TLS minimum version failed.", "");
        if (timeout_.count() == 0)
            ssl_socket_->handshake(boost::asio::ssl::stream_base::client);
        else
        {
            check_timeout();
            bool complete = false, failed = false;
            error_code ec;
            ssl_socket_->async_handshake(boost::asio::ssl::stream_base::client,
                [&complete, &failed, &ec](const error_code& error)
                { complete = !error; failed = !!error; ec = error; });
            wait_async(complete, failed, "TLS handshake timed out.", "TLS handshake failed.", ec);
        }
        ssl_ = true;
    }
    catch (const system_error& exc)
    {
        // TODO: perhaps the message is confusing
        throw dialog_error("Switching to SSL failed.", exc.code().message());
    }
}


void dialog_ssl::send(const string& line)
{
    if (!ssl_)
    {
        dialog::send(line);
        return;
    }

    if (timeout_.count() == 0)
        send_sync(*ssl_socket_, line);
    else
        send_async(*ssl_socket_, line);
}


string dialog_ssl::receive(bool raw)
{
    if (!ssl_)
        return dialog::receive(raw);

    try
    {
        if (timeout_.count() == 0)
            return receive_sync(*ssl_socket_, raw);
        else
            return receive_async(*ssl_socket_, raw);
    }
    catch (const system_error& exc)
    {
        throw dialog_error("Network receiving error.", exc.code().message());
    }
}


shared_ptr<dialog_ssl> dialog_ssl::to_ssl(const shared_ptr<dialog> dlg, const dialog_ssl::ssl_options_t& options)
{
    return make_shared<dialog_ssl>(*dlg, options);
}


string dialog_error::details() const
{
    return details_;
}

} // namespace mailio

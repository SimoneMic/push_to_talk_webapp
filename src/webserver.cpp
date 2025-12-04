// SPDX-FileCopyrightText: 2025 Humanoid Sensing and Perception, Istituto Italiano di Tecnologia
// SPDX-License-Identifier: BSD-3-Clause
// Author: Simone Micheletti

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <cstring>
#include <yarp/os/Network.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/sig/Sound.h>
#include <yarp/sig/SoundFile.h>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// ---- Get LAN IP ----
std::string get_lan_ip() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &serv.sin_addr);
    connect(sock, (sockaddr*)&serv, sizeof(serv));
    sockaddr_in name{};
    socklen_t namelen = sizeof(name);
    getsockname(sock, (sockaddr*)&name, &namelen);
    char buffer[32];
    inet_ntop(AF_INET, &name.sin_addr, buffer, sizeof(buffer));
    close(sock);
    return std::string(buffer);
}

// ---- Session: one per client ----
class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket)
        : ws_(std::move(socket)) {
            yarp_port_.open(local_port_name_);
            if (!yarp::os::Network::connect(local_port_name_, remote_port_name_))
            {
                std::cout << "Unable to connect: " << remote_port_name_ << " with " << local_port_name_ << std::endl;
            }
            // Set default parameters
            channels_ = 1;
            sampleRate_ = 48000;
        }

    void run() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if(ec) {
                std::cerr << "Accept error: " << ec.message() << std::endl;
                return;
            }
            std::cout << "Client connected" << std::endl;
            self->do_read();
        });
    }

private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::ofstream file_;
    std::string filename_ = "received_audio.webm";
    yarp::os::BufferedPort<yarp::sig::Sound> yarp_port_;
    std::string local_port_name_ = "/webserver/audio:o";
    std::string remote_port_name_ = "/SpeechTranscription_nws/audio:i";
    std::vector<char> audio_bytes_;
    yarp::sig::Sound sound_;
    int channels_;
    int sampleRate_;

    void do_read() {
        ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->on_read(ec, bytes);
        });
    }

    void on_read(beast::error_code ec, std::size_t /*bytes*/) {
        if(ec == websocket::error::closed) return;
        if(ec) {
            std::cerr << "Read error: " << ec.message() << std::endl;
            return;
        }

        if(ws_.got_text()) {
            std::string msg = beast::buffers_to_string(buffer_.data());

            if(msg == "__NEW_RECORDING__") {
                // Reset the YARP Sound for a new recording
                sound_.resize(channels_, 0);
                std::cout << "Starting new recording" << std::endl;
            }
            else if(msg == "__STOP_RECORDING__") {
                std::cout << "Recording finished: "
                          << sound_.getSamples() << " samples, "
                          << sound_.getChannels() << " channels" << std::endl;
                auto &msg = yarp_port_.prepare();
                msg.clear();
                msg = sound_;
                yarp_port_.write();
                sound_.clear();
            }

        } else { // binary PCM chunk
            auto data = buffer_.data();
            size_t num_bytes = data.size();

            if(num_bytes % 2 != 0) {
                std::cerr << "Warning: odd number of bytes in PCM chunk" << std::endl;
            }

            const int16_t* pcm = reinterpret_cast<const int16_t*>(data.data());
            size_t num_samples = num_bytes / 2;

            size_t old_samples_per_channel = sound_.getSamples();
            size_t new_samples_per_channel = num_samples / channels_;

            // Resize Sound to accommodate new samples
            sound_.resize(channels_, old_samples_per_channel + new_samples_per_channel);

            // Append samples channel by channel
            for(size_t i = 0; i < num_samples; ++i) {
                int channel = i % channels_;
                size_t sample_idx = old_samples_per_channel + i / channels_;
                sound_.getChannel(channel)[sample_idx].get() = pcm[i]; // store raw PCM
            }
        }

        buffer_.consume(buffer_.size());
        do_read(); // continue reading
    }



    bool finalize_sound() {
        if(audio_bytes_.empty()) return false;
        if(!yarp::sig::file::read_bytestream(
                sound_,
                audio_bytes_.data(),
                audio_bytes_.size(),
                ".mp3")) {
            std::cerr << "Failed to decode MP3 to YARP sound" << std::endl;
            return false;
        }
        std::cout << "Decoded audio into YARP sound: "
                  << sound_.getSamples() << " samples, "
                  << sound_.getChannels() << " channels" << std::endl;
        return true;
    }
};

// ---- Listener: accepts new connections ----
class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(net::io_context& ioc, tcp::endpoint endpoint)
        : acceptor_(ioc), socket_(ioc)
    {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if(ec) { std::cerr << "Open error: " << ec.message() << std::endl; return; }
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        if(ec) { std::cerr << "Bind error: " << ec.message() << std::endl; return; }
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if(ec) { std::cerr << "Listen error: " << ec.message() << std::endl; return; }
    }

    void run() { do_accept(); }

private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;

    void do_accept() {
        acceptor_.async_accept(socket_, [self = shared_from_this()](beast::error_code ec) {
            if(!ec) std::make_shared<Session>(std::move(self->socket_))->run();
            self->do_accept(); // keep accepting
        });
    }
};

int main() {
    try {
        yarp::os::Network yarp;
        uint16_t ws_port = 8000;
        if(const char* env = std::getenv("WS_PORT")) ws_port = std::stoi(env);

        std::string ip = get_lan_ip();
        std::string ws_url = "ws://" + ip + ":" + std::to_string(ws_port);
        std::cout << "Generating config.js with WS_URL = " << ws_url << std::endl;
        std::ofstream config("../webpage/config.js");
        config << "window.APP_CONFIG = { WS_URL: \"" << ws_url << "\" };";
        config.close();

        net::io_context ioc{1};
        auto endpoint = tcp::endpoint(tcp::v4(), ws_port);
        std::make_shared<Listener>(ioc, endpoint)->run();
        std::cout << "Server listening on ws://0.0.0.0:" << ws_port << " (LAN IP: " << ip << ")" << std::endl;

        ioc.run(); // start event loop
    } catch(std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
    }
}
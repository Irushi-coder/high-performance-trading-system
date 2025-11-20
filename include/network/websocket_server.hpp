#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include "network/tcp_server.hpp"
#include <string>
#include <vector>
#include <cstring>

namespace trading {
namespace network {

/**
 * Simple WebSocket server for real-time dashboard updates.
 * Implements minimal WebSocket protocol for browser connections.
 */
class WebSocketServer {
public:
    WebSocketServer(uint16_t port) : tcpServer_(port) {}

    bool start() {
        tcpServer_.setMessageCallback([this](const std::string& msg, socket_t client) {
            handleMessage(msg, client);
        });
        return tcpServer_.start();
    }

    void stop() {
        tcpServer_.stop();
    }

    // Broadcast JSON message to all connected clients
    void broadcast(const std::string& jsonMessage) {
        std::string frame = createWebSocketFrame(jsonMessage);
        tcpServer_.broadcast(frame);
    }

    size_t getClientCount() const {
        return tcpServer_.getClientCount();
    }

private:
    TCPServer tcpServer_;
    std::vector<socket_t> wsClients_;

    void handleMessage(const std::string& msg, socket_t client) {
        // Check if it's a WebSocket handshake
        if (msg.find("GET") == 0 && msg.find("Upgrade: websocket") != std::string::npos) {
            handleHandshake(msg, client);
        }
        // Otherwise it's a WebSocket frame (we'll ignore client messages for now)
    }

    void handleHandshake(const std::string& request, socket_t client) {
        // Extract WebSocket key
        size_t keyPos = request.find("Sec-WebSocket-Key: ");
        if (keyPos == std::string::npos) return;

        keyPos += 19; // Length of "Sec-WebSocket-Key: "
        size_t keyEnd = request.find("\r\n", keyPos);
        std::string key = request.substr(keyPos, keyEnd - keyPos);

        // Create accept key (simplified - in production use proper SHA-1)
        std::string acceptKey = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        
        // Build handshake response
        std::string response = 
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + base64Encode(acceptKey) + "\r\n"
            "\r\n";

        tcpServer_.sendMessage(client, response);
        wsClients_.push_back(client);
    }

    std::string createWebSocketFrame(const std::string& payload) {
        std::vector<uint8_t> frame;
        
        // Byte 0: FIN (1) + Opcode (1 = text)
        frame.push_back(0x81);
        
        // Byte 1: Mask (0) + Payload length
        size_t len = payload.length();
        if (len < 126) {
            frame.push_back(static_cast<uint8_t>(len));
        } else if (len < 65536) {
            frame.push_back(126);
            frame.push_back((len >> 8) & 0xFF);
            frame.push_back(len & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back((len >> (i * 8)) & 0xFF);
            }
        }
        
        // Payload
        frame.insert(frame.end(), payload.begin(), payload.end());
        
        return std::string(frame.begin(), frame.end());
    }

    // Simple base64 encoding (for WebSocket handshake)
    std::string base64Encode(const std::string& input) {
        static const char* base64Chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string output;
        int val = 0;
        int valb = -6;
        
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                output.push_back(base64Chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        
        if (valb > -6) {
            output.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        
        while (output.size() % 4) {
            output.push_back('=');
        }
        
        return output;
    }
};

} // namespace network
} // namespace trading

#endif // WEBSOCKET_SERVER_HPP
#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

namespace trading {
namespace network {

/**
 * Simple TCP Server for receiving FIX messages.
 */
class TCPServer {
public:
    using MessageCallback = std::function<void(const std::string&, socket_t)>;

    TCPServer(uint16_t port) 
        : port_(port)
        , running_(false)
        , serverSocket_(INVALID_SOCKET)
    {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~TCPServer() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    /**
     * Start the server.
     */
    bool start() {
        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ == INVALID_SOCKET) {
            return false;
        }

        // Set socket options
        int opt = 1;
#ifdef _WIN32
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, 
                   (char*)&opt, sizeof(opt));
#else
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt));
#endif

        // Bind to port
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port_);

        if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closeSocket(serverSocket_);
            return false;
        }

        // Listen for connections
        if (listen(serverSocket_, 10) == SOCKET_ERROR) {
            closeSocket(serverSocket_);
            return false;
        }

        running_ = true;
        acceptThread_ = std::thread(&TCPServer::acceptLoop, this);

        return true;
    }

    /**
     * Stop the server.
     */
    void stop() {
        running_ = false;

        if (serverSocket_ != INVALID_SOCKET) {
            closeSocket(serverSocket_);
            serverSocket_ = INVALID_SOCKET;
        }

        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }

        for (auto& thread : clientThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        clientThreads_.clear();
    }

    /**
     * Set callback for received messages.
     */
    void setMessageCallback(MessageCallback callback) {
        messageCallback_ = std::move(callback);
    }

    /**
     * Send message to a specific client.
     */
    bool sendMessage(socket_t clientSocket, const std::string& message) {
        if (clientSocket == INVALID_SOCKET) {
            return false;
        }

#ifdef _WIN32
        int result = send(clientSocket, message.c_str(), message.length(), 0);
#else
        ssize_t result = send(clientSocket, message.c_str(), message.length(), 0);
#endif

        return result != SOCKET_ERROR;
    }

    /**
     * Broadcast message to all connected clients.
     */
    void broadcast(const std::string& message) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (socket_t client : clients_) {
            sendMessage(client, message);
        }
    }

    /**
     * Get number of connected clients.
     */
    size_t getClientCount() const {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        return clients_.size();
    }

private:
    uint16_t port_;
    std::atomic<bool> running_;
    socket_t serverSocket_;
    std::thread acceptThread_;
    std::vector<std::thread> clientThreads_;
    std::vector<socket_t> clients_;
    mutable std::mutex clientsMutex_;
    MessageCallback messageCallback_;

    void acceptLoop() {
        while (running_) {
            sockaddr_in clientAddr{};
#ifdef _WIN32
            int clientAddrSize = sizeof(clientAddr);
#else
            socklen_t clientAddrSize = sizeof(clientAddr);
#endif

            socket_t clientSocket = accept(serverSocket_, 
                                          (sockaddr*)&clientAddr, 
                                          &clientAddrSize);

            if (clientSocket != INVALID_SOCKET) {
                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    clients_.push_back(clientSocket);
                }

                clientThreads_.emplace_back(
                    &TCPServer::handleClient, this, clientSocket
                );
            }
        }
    }

    void handleClient(socket_t clientSocket) {
        const size_t BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];

        while (running_) {
#ifdef _WIN32
            int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
#else
            ssize_t bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
#endif

            if (bytesReceived <= 0) {
                break; // Client disconnected or error
            }

            buffer[bytesReceived] = '\0';
            std::string message(buffer, bytesReceived);

            if (messageCallback_) {
                messageCallback_(message, clientSocket);
            }
        }

        // Remove client
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.erase(
                std::remove(clients_.begin(), clients_.end(), clientSocket),
                clients_.end()
            );
        }

        closeSocket(clientSocket);
    }

    void closeSocket(socket_t socket) {
#ifdef _WIN32
        closesocket(socket);
#else
        close(socket);
#endif
    }
};

} // namespace network
} // namespace trading

#endif // TCP_SERVER_HPP
#pragma once

#include <uvw.hpp>
#include <v8.h>
#include "commonHeaders.h"

#include "httpConfig.h"
#include "abstractRequestProcessor.h"
#include "httpConnection.h"

namespace nex {

typedef std::function<void(const uvw::ErrorEvent& err)> ErrorCallback;


class EmbeddedHttp {

public:

    using Deleter = void(*)(EmbeddedHttp*);
    static std::shared_ptr<EmbeddedHttp> createInstance(
        std::shared_ptr<AbstractRequestProcessor> requestProc,
        v8::Isolate* isolate,
        ErrorCallback onError = nullptr,
        std::shared_ptr<HttpServerConfig> config = std::make_shared<HttpServerConfig>()
    );

    void listen(const std::string& ip, uint32_t port);
    void close() noexcept;

    [[nodiscard]] bool isActive() const noexcept;

private:

    explicit EmbeddedHttp(
        std::shared_ptr<uvw::Loop> eventLoop,
        v8::Isolate* isolate,
        std::shared_ptr<AbstractRequestProcessor> requestProc,
        std::shared_ptr<HttpServerConfig> config,
        ErrorCallback onError
    );
    static void deleter(EmbeddedHttp* ptr) noexcept;

    [[nodiscard]] bool isClosing() const noexcept;
    [[nodiscard]] bool isShuttingDown() const noexcept;

    void setupTcpHandle();

    static void handleConnection(const uvw::ListenEvent&, uvw::TCPHandle& server);
    static void handleServerError(const uvw::ErrorEvent& err, uvw::TCPHandle& server);
    static void handleShutdown(const uvw::ShutdownEvent&, uvw::TCPHandle& server);
    static void handleClose(const uvw::CloseEvent&, uvw::TCPHandle& server);

    std::shared_ptr<uvw::Loop> loop;
    std::shared_ptr<uvw::TCPHandle> tcpHandle{nullptr};
    std::shared_ptr<EmbeddedHttp> thisRef{this, noop<EmbeddedHttp>};
    std::shared_ptr<HttpServerConfig> config;

    std::shared_ptr<AbstractRequestProcessor> requestProcessor;
    ErrorCallback errorCallback;

    v8::Isolate* isolate;

    bool active = false;
    bool closing = false;
    bool shuttingDown = false;
    bool deleterCalled_ = false;

};

}

#include "embeddedHttp.h"

namespace nex {

    std::shared_ptr<EmbeddedHttp> EmbeddedHttp::createInstance(
        std::shared_ptr<AbstractRequestProcessor> requestProc,
        v8::Isolate* isolate,
        ErrorCallback onError,
        std::shared_ptr<HttpServerConfig> config
    ) {
        return std::shared_ptr<EmbeddedHttp>(
            new EmbeddedHttp(
                uvw::Loop::getDefault(),
                isolate,
                std::move(requestProc),
                std::move(config),
                std::move(onError)
            ),
            EmbeddedHttp::deleter
        );
    }

    EmbeddedHttp::EmbeddedHttp(
        std::shared_ptr<uvw::Loop> eventLoop,
        v8::Isolate* isolate,
        std::shared_ptr<AbstractRequestProcessor> requestProc,
        std::shared_ptr<HttpServerConfig> config,
        ErrorCallback onError
    ):
        loop(std::move(eventLoop)),
        config(std::move(config)),
        requestProcessor(std::move(requestProc)),
        errorCallback(std::move(onError)),
        isolate(isolate)
        {}

    void EmbeddedHttp::listen(const std::string &ip, uint32_t port) {
        if (isActive())
            throw std::runtime_error("Server already listening.");

        if (isClosing() || isShuttingDown())
            throw std::runtime_error("Server is closing or shutting down");

        setupTcpHandle();

        tcpHandle->bind(ip, port);
        tcpHandle->listen();

        active = true;
    }

    void EmbeddedHttp::close() noexcept {
        if (isClosing() || isShuttingDown())
            return;

        if (tcpHandle) {
            if (isActive()) {
                shuttingDown = true;
                tcpHandle->shutdown();

                return;
            }

            closing = true;
            tcpHandle->close();
        }
    }

    void EmbeddedHttp::setupTcpHandle() {
        tcpHandle = loop->resource<uvw::TCPHandle>();
        tcpHandle->data(thisRef);

        tcpHandle->on<uvw::ErrorEvent>(handleServerError);
        tcpHandle->on<uvw::ListenEvent>(handleConnection);
        tcpHandle->on<uvw::ShutdownEvent>(handleShutdown);
        tcpHandle->on<uvw::CloseEvent>(handleClose);
    }

    bool EmbeddedHttp::isActive() const noexcept {
        return active;
    }

    bool EmbeddedHttp::isClosing() const noexcept {
        return closing;
    }

    bool EmbeddedHttp::isShuttingDown() const noexcept {
        return shuttingDown;
    }

    void EmbeddedHttp::handleConnection(const uvw::ListenEvent &, uvw::TCPHandle &server) {
        auto client = server.loop().resource<uvw::TCPHandle>();
        auto http = server.data<EmbeddedHttp>();
        auto requestProc = http->requestProcessor;

        [[maybe_unused]] auto httpConnection = new HttpConnection(
            http->loop,
            http->isolate,
            client,
            requestProc,
            http->config
        );

        server.accept(*client);
        client->read();
    }

    void EmbeddedHttp::handleServerError(const uvw::ErrorEvent &err, uvw::TCPHandle &server) {
        auto http = server.data<EmbeddedHttp>();

        if (http->errorCallback)
            http->errorCallback(err);

        if (!http->isShuttingDown() && !http->isClosing())
            http->close();
        else if (http->isShuttingDown())
            server.close();
    }

    void EmbeddedHttp::handleShutdown(const uvw::ShutdownEvent &, uvw::TCPHandle &server) {
        auto http = server.data<EmbeddedHttp>();

        http->shuttingDown = false;
        if (!http->isClosing())
            server.close();
    }

    void EmbeddedHttp::handleClose(const uvw::CloseEvent&, uvw::TCPHandle& server) {
        auto http = server.data<EmbeddedHttp>();

        http->tcpHandle.reset();

        http->shuttingDown = false;
        http->closing = false;
        http->active = false;

        if (http->deleterCalled_) {
            auto ptr = http.get();
            http.reset();

            delete ptr;
        }
    }

    void EmbeddedHttp::deleter(EmbeddedHttp* ptr) noexcept {
        if (ptr->isActive()) {
            if (!ptr->isClosing() && !ptr->isShuttingDown())
                ptr->close();

            ptr->deleterCalled_ = true;
        } else {
            delete ptr;
        }
    }




}

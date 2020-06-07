#pragma once


#include <picohttpparser.h>
#include <uvw.hpp>

#include "commonHeaders.h"
#include "httpConfig.h"

#include "abstractRequestProcessor.h"
#include "request.h"
#include "response.h"
#include "middleware.h"

namespace nex {

typedef std::chrono::duration<uint64_t, std::milli> Time;

class ParseError : public std::exception {public:using std::exception::exception;};
class RequestHeadersTooLargeError : public std::exception {public:using std::exception::exception;};
class NeedMoreDataError : public std::exception {public:using std::exception::exception;};
class HttpVersionUnsupportedError : public std::exception {public:using std::exception::exception;};
class UnknownHTTPMethodError : public std::exception {public:using std::exception::exception;};

class Request;
class Response;
class AbstractRequestProcessor;

class HttpConnection {

    friend class EmbeddedHttp;
    friend class Request;
    friend class Response;

    HttpConnection(
            std::shared_ptr<uvw::Loop> eventLoop,
            v8::Isolate* isolate,
            std::shared_ptr<uvw::TCPHandle> client,
            std::shared_ptr<AbstractRequestProcessor> requestProc,
            std::shared_ptr<HttpServerConfig> config
    );

    static void handleClientError(const uvw::ErrorEvent& err, uvw::TCPHandle& client);
    static void handleDataEnd(const uvw::EndEvent&, uvw::TCPHandle& client);
    static void handleData(const uvw::DataEvent& data, uvw::TCPHandle& client);
    static void handleShutdown(const uvw::ShutdownEvent&, uvw::TCPHandle& client);
    static void handleClose(const uvw::CloseEvent&, uvw::TCPHandle& client);

    static void handleRequestTimeout(const uvw::TimerEvent&, uvw::TimerHandle& timer);
    static void handleResponseTimeout(const uvw::TimerEvent&, uvw::TimerHandle& timer);
    static void handleKeepAliveTimeout(const uvw::TimerEvent&, uvw::TimerHandle& timer);
    static void handleRequestTimeoutClose(const uvw::CloseEvent&, uvw::TimerHandle& timer);
    static void handleResponseTimeoutClose(const uvw::CloseEvent&, uvw::TimerHandle& timer);
    static void handleKeepAliveTimeoutClose(const uvw::CloseEvent&, uvw::TimerHandle& timer);

    static void parseHeaders(
            const char* buffer,
            uint32_t bufferStart,
            uint32_t bufferLength,
            std::vector<phr_header>& headers,
            std::string& path,
            HttpMethod& method,
            uint32_t& minorVersion,
            uint32_t& bodyStart
    );

    static inline void packHeaders(
            const std::vector<phr_header>& headers,
            const std::shared_ptr<Request>& req
    );

    template<class T>
    static std::shared_ptr<HttpConnection> getConnection(T& client) {
        return std::move(client.template data<HttpConnection>());
    }

    void close();
    void end();
    void eliminate();

    void parseRequest(const std::string& buffer, uint32_t& bufferPosition);
    void processNextRequest();
    void updateRequestBodyBuffer(const std::string& buffer, uint32_t& bufferPosition);
    [[nodiscard]] bool isRequestLimitExceeded() const;

    void startRequestTimer();
    void startResponseTimer();
    void startKeepAliveTimer();
    void stopRequestTimer();
    void stopResponseTimer();
    void stopKeepAliveTimer();
    void closeTimeouts() noexcept;

    std::shared_ptr<Request> createRequest(
            const std::vector<phr_header>& headers,
            const std::string& path,
            HttpMethod method,
            uint32_t minorVersion
    );
    std::shared_ptr<Request> createRequest(uint32_t errorCode, uint32_t minorVersion = 1);
    std::shared_ptr<Response> createResponse(const std::shared_ptr<Request>& req);

    std::shared_ptr<uvw::Loop> loop;
    std::shared_ptr<uvw::TCPHandle> client;
    std::shared_ptr<uvw::TimerHandle> requestTimeout{nullptr},
            responseTimeout{nullptr}, keepAliveTimeout{nullptr};
    std::shared_ptr<HttpConnection> thisRef{this, noop<HttpConnection>};
    std::shared_ptr<HttpServerConfig> config;

    std::shared_ptr<Request> request;
    std::queue<std::shared_ptr<Request>> requestQueue;
    std::shared_ptr<Response> response;

    std::shared_ptr<AbstractRequestProcessor> requestProcessor;
    v8::Isolate* isolate;

    uint32_t requestsAccepted = 0;
    uint32_t lastContentLeft = 0;

    std::string headerBuffer;

    bool active = true;

    bool needMoreDataToParseHeaders = false;
    bool needMoreDataToGetBody = false;

    bool hasActiveRequest = false;

    bool shuttingDown = false;
    bool closing = false;
    bool requestTimeoutClosing = false;
    bool responseTimeoutClosing = false;
    bool keepAliveTimeoutClosing = false;

};  // class HttpConnection

}


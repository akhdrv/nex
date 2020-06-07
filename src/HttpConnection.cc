#include "httpConnection.h"

namespace nex {

    HttpConnection::HttpConnection(
            std::shared_ptr<uvw::Loop> eventLoop,
            v8::Isolate* isolate,
            std::shared_ptr<uvw::TCPHandle> clientHandle,
            std::shared_ptr<AbstractRequestProcessor> requestProc,
            std::shared_ptr<HttpServerConfig> configuration
    ) :
            loop(std::move(eventLoop)),
            client(std::move(clientHandle)),
            config(std::move(configuration)),
            requestProcessor(std::move(requestProc)),
            isolate(isolate)
    {
        startKeepAliveTimer();

        client->data(thisRef);

        client->on<uvw::CloseEvent>(handleClose);
        client->on<uvw::ErrorEvent>(handleClientError);
        client->on<uvw::EndEvent>(handleDataEnd);
        client->on<uvw::DataEvent>(handleData);
        client->on<uvw::ShutdownEvent>(handleShutdown);
    }

    void HttpConnection::close() {
        closeTimeouts();

        if (closing || shuttingDown)
            return;

        if (request)
            request->invalidate();
        if (response)
            response->invalidate();

        if (client) {
            if (active) {
                active = false;
                shuttingDown = true;
                client->shutdown();

                return;
            }

            closing = true;
            client->close();
        }
    }

    void HttpConnection::eliminate() {
        if (!requestTimeout && !responseTimeout && !keepAliveTimeout && !client) {
            delete this;
        }
    }

    void HttpConnection::end() {
        stopResponseTimer();
        hasActiveRequest = false;

        if (request)
            request->invalidate();
        if (response)
            response->invalidate();

        request.reset();
        response.reset();

        if (!config->persistentConnections) {
            close();
            return;
        }

        processNextRequest();
    }

    void HttpConnection::parseHeaders(
            const char* buffer,
            uint32_t bufferStart,
            uint32_t bufferLength,
            std::vector<phr_header>& headers,
            std::string& path,
            HttpMethod& method,
            uint32_t& minorVersion,
            uint32_t& bodyStart
    ) {
        const char *httpMethod, *uriPath;
        int parseStatus, httpVersion;
        phr_header headersRaw[100];
        size_t previousBufferLength = 0, methodLength,
                pathLength, headersCount = 100;

        parseStatus = phr_parse_request(
                buffer + bufferStart, bufferLength, &httpMethod, &methodLength,
                &uriPath, &pathLength, &httpVersion, headersRaw, &headersCount,
                previousBufferLength
        );

        if (parseStatus > 0) {
            bodyStart = static_cast<uint32_t>(parseStatus);
            minorVersion = static_cast<uint32_t>(httpVersion);
            path = std::string(uriPath, pathLength);
            headers = std::vector<phr_header>(headersRaw, headersRaw + headersCount);

            try {
                method = parseMethod(std::string(httpMethod, methodLength));
            } catch (const std::runtime_error&) {
                throw UnknownHTTPMethodError();
            }

            if (minorVersion != 0 && minorVersion != 1) {
                throw HttpVersionUnsupportedError();
            }

            return;
        } else if (parseStatus == -1) {
            throw ParseError();
        } else if (parseStatus == -2) {
            if (bufferLength - bufferStart > 1024 * 16)
                throw RequestHeadersTooLargeError();
            throw NeedMoreDataError();
        }
    }

    void HttpConnection::parseRequest(const std::string& buffer, uint32_t& bufferPosition) {
        std::shared_ptr<Request> req;
        std::vector<phr_header> rawHeaders;
        std::string path;
        HttpMethod method;
        uint32_t bodyStart = 0, minorVersion = 1;

        ++requestsAccepted;
        try {
            parseHeaders(
                buffer.data(),
                bufferPosition,
                buffer.length() - bufferPosition,
                rawHeaders,
                path,
                method,
                minorVersion,
                bodyStart
            );
        } catch (const ParseError&) {
            req = createRequest(400, minorVersion);
            requestQueue.push(std::move(req));

            throw std::exception();
        } catch (const RequestHeadersTooLargeError&) {
            req = createRequest(413, minorVersion);
            requestQueue.push(std::move(req));

            throw std::exception();
        } catch (const NeedMoreDataError &) {
            needMoreDataToParseHeaders = true;
            if (bufferPosition < buffer.length()) {
                headerBuffer = std::string(buffer.begin() + bufferPosition, buffer.end());
                bufferPosition = buffer.length();
            }

            return;
        } catch (const UnknownHTTPMethodError &) {
            req = createRequest(405, minorVersion);
            requestQueue.push(std::move(req));

            throw std::exception();
        } catch (const HttpVersionUnsupportedError &) {
            req = createRequest(505, 1);
            requestQueue.push(std::move(req));

            throw std::exception();
        } catch (const std::exception&) {
            req = createRequest(500, minorVersion);
            requestQueue.push(std::move(req));

            throw std::exception();
        }

        bufferPosition += bodyStart;

        req = createRequest(rawHeaders, path, method, minorVersion);

        if (path.length() > config->maxPathLength) {
            req->requestError = 414;
        }

        auto contentLengthRaw = std::get_if<std::string>(&req->headers["Content-Length"]);
        if (!contentLengthRaw) {
            req->isFullData = true;
            requestQueue.push(std::move(req));

            return;
        }

        uint32_t contentLength;

        try {
            contentLength = std::stoll(*contentLengthRaw);
        } catch (const std::exception&) {
            req->requestError = 411;
            requestQueue.push(std::move(req));

            throw std::exception();
        }

        if (contentLength > config->maxRequestBodyLength) {
            req->requestError = 413;
            requestQueue.push(std::move(req));

            throw std::exception();
        }

        req->contentLength = contentLength;
        requestQueue.push(std::move(req));

        updateRequestBodyBuffer(buffer, bufferPosition);
    }

    void HttpConnection::updateRequestBodyBuffer(const std::string& buffer, uint32_t& bufferPosition) {
        std::shared_ptr<Request> req;

        if (!requestQueue.empty()) {
            req = requestQueue.back();
        } else if (request) {
            req = request;
        } else {
            // Pass data if request ignored data receive and is completed
            auto bytesReceived = std::min(lastContentLeft, static_cast<uint32_t>(buffer.length() - bufferPosition));
            bufferPosition += lastContentLeft;
            lastContentLeft -= bytesReceived;
            return;
        }

        auto contentLeft = req->contentLength - req->bodyOctetsReceived;
        auto data = std::string(
            buffer.begin() + bufferPosition,
            buffer.begin() + std::min(
                bufferPosition + contentLeft,
                static_cast<uint32_t>(buffer.length())
            )
        );
        auto bytesReceived = data.length();
        bufferPosition += bytesReceived;
        lastContentLeft = contentLeft - bytesReceived;

        req->handleData(data);

        if (bytesReceived == contentLeft) {
            req->handleDataEnd();
        } else {
            needMoreDataToGetBody = true;
        }
    }

    void HttpConnection::processNextRequest() {
        if (hasActiveRequest) {
            return;
        }

        if (requestQueue.empty()) {
            startKeepAliveTimer();
            return;
        }

        request = std::move(requestQueue.front());
        requestQueue.pop();
        response = createResponse(request);

        hasActiveRequest = true;

        if (request->requestError) {
            response->end();
            return;
        }

        startResponseTimer();
        requestProcessor->process(request, response);
    }

    void HttpConnection::handleData(const uvw::DataEvent &data, uvw::TCPHandle &client) {
        auto httpConnection = getConnection(client);
        auto buffer = httpConnection->headerBuffer + std::string(data.data.get(), data.length);
        uint32_t bufferPos = 0;

        httpConnection->headerBuffer.clear();
        httpConnection->needMoreDataToParseHeaders = false;

        httpConnection->stopKeepAliveTimer();
        httpConnection->stopRequestTimer();

        if (httpConnection->needMoreDataToGetBody) {
            httpConnection->needMoreDataToGetBody = false;

            httpConnection->updateRequestBodyBuffer(buffer, bufferPos);
        }

        while (bufferPos < buffer.length()) {
            try {
                httpConnection->parseRequest(buffer, bufferPos);
            } catch (const std::exception&) {
                client.stop();
                break;
            }
            if (httpConnection->isRequestLimitExceeded()) {
                break;
            }
        }

        httpConnection->processNextRequest();

        if (httpConnection->isRequestLimitExceeded() && !httpConnection->needMoreDataToGetBody) {
            client.stop();
        }

        if (httpConnection->needMoreDataToGetBody || httpConnection->needMoreDataToParseHeaders) {
            httpConnection->startRequestTimer();
        }
    }

    void HttpConnection::packHeaders(
            const std::vector<phr_header>& rawHeaders,
            const std::shared_ptr<Request>& req
    ) {
        std::string name, value;

        for (const auto & rawHeader : rawHeaders) {
            if (rawHeader.name != nullptr) {
                if (rawHeader.value == nullptr)
                    continue;

                name = std::string(rawHeader.name, rawHeader.name_len);
                value = std::string(rawHeader.value, rawHeader.value_len);

                req->appendHeader(name, std::move(value));
            } else if (rawHeader.value != nullptr) {
                value = std::string(rawHeader.value, rawHeader.value_len);

                if (!name.empty()) {
                    req->appendHeader(name, std::move(value));
                }
            }
        }
    }

    std::shared_ptr<Request> HttpConnection::createRequest(
            const std::vector<phr_header>& headers,
            const std::string& path,
            HttpMethod method,
            uint32_t minorVersion
    ) {
        auto req = std::shared_ptr<Request>(
                new Request(thisRef, isolate, method, path, minorVersion)
        );

        packHeaders(headers, req);

        return req;
    }

    std::shared_ptr<Request> HttpConnection::createRequest(uint32_t errorCode, uint32_t minorVersion) {
        auto req = std::shared_ptr<Request>(new Request(errorCode));
        req->minorVersion = minorVersion;

        return req;
    }

    std::shared_ptr<Response> HttpConnection::createResponse(const std::shared_ptr<Request>& req) {
        auto res = std::shared_ptr<Response>(
                new Response(thisRef, isolate, req->minorVersion)
        );

        if (req->requestError) {
            res->setStatus(req->requestError);
        }

        return res;
    }

    bool HttpConnection::isRequestLimitExceeded() const {
        if (config->persistentConnections)
            return config->maxRequestsPerConnection < requestsAccepted;

        return requestsAccepted > 0;
    }

    void HttpConnection::handleClientError(const uvw::ErrorEvent &err, uvw::TCPHandle &client) {
        auto httpConnection = getConnection(client);
        if (httpConnection->hasActiveRequest) {
            httpConnection->request->handleDataEnd();
            httpConnection->response->invalidate();
        }

        httpConnection->shuttingDown = false;
        httpConnection->close();
    }

    void HttpConnection::handleDataEnd(const uvw::EndEvent &, uvw::TCPHandle &client) {
        auto httpConnection = getConnection(client);

        std::shared_ptr<Request> req;

        if (!httpConnection->requestQueue.empty()) {
            req = httpConnection->requestQueue.back();
        } else if (httpConnection->request) {
            req = httpConnection->request;
        } else {
            return;
        }

        req->handleDataEnd();
    }

    void HttpConnection::handleShutdown(const uvw::ShutdownEvent &, uvw::TCPHandle &client) {
        auto httpConnection = getConnection(client);

        httpConnection->shuttingDown = false;
        httpConnection->close();
    }

    void HttpConnection::handleClose(const uvw::CloseEvent&, uvw::TCPHandle& client) {
        auto httpConnection = getConnection(client);

        httpConnection->closing = false;
        httpConnection->client.reset();
        httpConnection->eliminate();
    }

    void HttpConnection::startRequestTimer() {
        if (!config->requestTimeout) {
            return;
        }

        if (!requestTimeout) {
            requestTimeout = loop->resource<uvw::TimerHandle>();
            requestTimeout->data(thisRef);

            requestTimeout->on<uvw::TimerEvent>(handleRequestTimeout);
            requestTimeout->once<uvw::CloseEvent>(handleRequestTimeoutClose);
        }

        requestTimeout->start(Time(config->requestTimeout),Time(0));
    }

    void HttpConnection::startResponseTimer() {
        if (!config->responseTimeout) {
            return;
        }

        if (!responseTimeout) {
            responseTimeout = loop->resource<uvw::TimerHandle>();
            responseTimeout->data(thisRef);

            responseTimeout->on<uvw::TimerEvent>(handleResponseTimeout);
            responseTimeout->once<uvw::CloseEvent>(handleResponseTimeoutClose);
        }

        responseTimeout->start(Time(config->responseTimeout),Time(0));
    }

    void HttpConnection::startKeepAliveTimer() {
        if (!config->keepAliveTimeout || !config->persistentConnections) {
            return;
        }

        if (!keepAliveTimeout) {
            keepAliveTimeout = loop->resource<uvw::TimerHandle>();
            keepAliveTimeout->data(thisRef);

            keepAliveTimeout->on<uvw::TimerEvent>(handleKeepAliveTimeout);
            keepAliveTimeout->once<uvw::CloseEvent>(handleKeepAliveTimeoutClose);
        }

        keepAliveTimeout->start(Time(config->keepAliveTimeout),Time(0));
    }

    void HttpConnection::stopRequestTimer() {
        if (requestTimeout && !requestTimeoutClosing)
            requestTimeout->stop();
    }

    void HttpConnection::stopResponseTimer() {
        if (responseTimeout && !responseTimeoutClosing)
            responseTimeout->stop();
    }

    void HttpConnection::stopKeepAliveTimer() {
        if (keepAliveTimeout && !keepAliveTimeoutClosing)
            keepAliveTimeout->stop();
    }

    void HttpConnection::handleRequestTimeout(const uvw::TimerEvent&, uvw::TimerHandle& timer) {
        auto httpConnection = getConnection(timer);

        httpConnection->client->stop();
        if (httpConnection->needMoreDataToParseHeaders) {
            auto req = httpConnection->createRequest(408);
            httpConnection->requestQueue.push(req);

            httpConnection->processNextRequest();

            return;
        }

        if (httpConnection->needMoreDataToGetBody) {
            if (!httpConnection->requestQueue.empty()) {
                auto req = httpConnection->requestQueue.back();

                req->requestError = 408;
            } else {
                if (!httpConnection->response) {
                    return;
                }

                auto res = httpConnection->response;
                res->setStatus(408);
                httpConnection->request->handleDataEnd();

                if (res && res->isAlive) {
                    res->end();
                }
            }
        }
    }

    void HttpConnection::handleResponseTimeout(const uvw::TimerEvent&, uvw::TimerHandle& timer) {
        auto httpConnection = getConnection(timer);

        if (!httpConnection->response) {
            return;
        }

        httpConnection->response->setStatus(500);
        httpConnection->response->end();
    }

    void HttpConnection::handleKeepAliveTimeout(const uvw::TimerEvent&, uvw::TimerHandle& timer) {
        auto httpConnection = getConnection(timer);

        httpConnection->client->stop();
        if (httpConnection->hasActiveRequest || !httpConnection->requestQueue.empty()) {
            return;
        }

        httpConnection->close();
    }

    void HttpConnection::handleRequestTimeoutClose(const uvw::CloseEvent&, uvw::TimerHandle& timer) {
        auto httpConnection = getConnection(timer);

        httpConnection->requestTimeoutClosing = false;
        httpConnection->requestTimeout.reset();
        httpConnection->eliminate();
    }

    void HttpConnection::handleResponseTimeoutClose(const uvw::CloseEvent&, uvw::TimerHandle& timer) {
        auto httpConnection = getConnection(timer);

        httpConnection->responseTimeoutClosing = false;
        httpConnection->responseTimeout.reset();
        httpConnection->eliminate();
    }

    void HttpConnection::handleKeepAliveTimeoutClose(const uvw::CloseEvent&, uvw::TimerHandle& timer) {
        auto httpConnection = getConnection(timer);

        httpConnection->keepAliveTimeoutClosing = false;
        httpConnection->keepAliveTimeout.reset();
        httpConnection->eliminate();
    }

    void HttpConnection::closeTimeouts() noexcept {
        if (requestTimeout && !requestTimeoutClosing) {
            requestTimeoutClosing = true;
            requestTimeout->close();
        }

        if (responseTimeout && !responseTimeoutClosing) {
            responseTimeoutClosing = true;
            responseTimeout->close();
        }

        if (keepAliveTimeout && !keepAliveTimeoutClosing) {
            keepAliveTimeoutClosing = true;
            keepAliveTimeout->close();
        }
    }

} // namespace nex
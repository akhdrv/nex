#pragma once

namespace nex {
    class HttpServerConfig {
    public:
        uint32_t responseTimeout = 15000;
        uint32_t requestTimeout = 5000;
        uint32_t keepAliveTimeout = 5000;
        uint32_t maxRequestBodyLength = 1024 * 1024 * 10;
        uint32_t maxRequestsPerConnection = 1000;
        uint32_t maxPathLength = 8 * 1024;
        bool persistentConnections = true;
        std::string protocol = "http";
    };
}

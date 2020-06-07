#pragma once


#include "request.h"
#include "response.h"

namespace nex {
    class Request;
    class Response;

    class AbstractRequestProcessor {
    public:
        virtual void process(std::shared_ptr<Request> req, std::shared_ptr<Response> res) = 0;
    };
}

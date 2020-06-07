#pragma once

#include "commonHeaders.h"

#include <node.h>
#include <node_object_wrap.h>

#include "abstractRequestProcessor.h"
#include "middleware.h"

#include "request.h"
#include "response.h"
#include "pathRegexp.h"

namespace nex {
typedef std::pair<PathRegExp, std::shared_ptr<AbstractMiddleware>> MiddlewareConfig;

using v8::Persistent;
using v8::Local;
using v8::Isolate;
using v8::Value;
using v8::Context;
using v8::Object;
using v8::Function;
using v8::FunctionTemplate;

class Request;
class Response;

class Router: public AbstractRequestProcessor, public ApplicationMiddleware {
public:
    explicit Router(v8::Isolate* isolate);

    void process(std::shared_ptr<Request> req, std::shared_ptr<Response> res) final;

    void use(
        HttpMethod method,
        bool canHandlePartial,
        const std::vector<std::string>& paramKeys,
        const std::string& path,
        const std::shared_ptr<AbstractMiddleware>& middleware
    );

    void emit(
        std::shared_ptr<Request> req,
        std::shared_ptr<Response> res,
        NextObject& next
    ) final;

    virtual ~Router() = default;

private:

    void fillConfig(HttpMethod method, const PathRegExp& re, std::shared_ptr<AbstractMiddleware> middleware);

    std::map<HttpMethod, std::shared_ptr<std::vector<MiddlewareConfig>>> methodToConfigs;

    v8::Isolate* isolate;
};


class Pipeline {
private:
    friend class Router;

    Pipeline(
        std::shared_ptr<Request> request,
        std::shared_ptr<Response> response,
        std::shared_ptr<std::vector<MiddlewareConfig>> middlewareList,
        v8::Isolate* isolate
    );

    Pipeline(
        std::shared_ptr<Request> request,
        std::shared_ptr<Response> response,
        std::shared_ptr<std::vector<MiddlewareConfig>> middlewareList,
        NextObject* next,
        v8::Isolate* isolate
    );

    void execute();
    void eliminate();
    void setNextObject();
    std::shared_ptr<AbstractMiddleware> getNext(bool nextRoute = false);
    std::shared_ptr<AbstractMiddleware> getNextErrorHandling();
    [[nodiscard]] bool isValid() const;

    NextObject next;
    NextObject* baseNext = nullptr;

    std::function<void()> eliminateCallback;
    std::function<void()>* baseEliminateCallback = nullptr;

    size_t current = 0;
    bool isBase;
    bool isHandled = false;

    std::shared_ptr<Request> req;
    std::shared_ptr<Response> res;
    std::shared_ptr<std::vector<MiddlewareConfig>> middlewareList;

    v8::Isolate* isolate;
};


class RouterMethods : public node::ObjectWrap {
protected:
    static void Use(const v8::FunctionCallbackInfo<v8::Value>& args);

    std::shared_ptr<Router> routerInstance{nullptr};
};

class RouterWrap: RouterMethods {
public:
    static void Init(v8::Isolate* isolate);
    static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);

private:
    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static v8::Global<v8::Function> constructor;
};

}
#include "router.h"

namespace nex {

    // Router

    Router::Router(v8::Isolate* isolate)
        : isolate(isolate) {}

    void Router::process(std::shared_ptr<Request> req, std::shared_ptr<Response> res) {
        auto method = req->getHttpMethod();
        if (!methodToConfigs[method]) {
            methodToConfigs[method] = std::make_shared<std::vector<MiddlewareConfig>>();
        }

        auto middlewareList = methodToConfigs[method];
        auto pipeline = new Pipeline(std::move(req), std::move(res), middlewareList, isolate);

        pipeline->execute();
    }

    void Router::use(
        HttpMethod method,
        bool canHandlePartial,
        const std::vector<std::string>& paramKeys,
        const std::string& path,
        const std::shared_ptr<AbstractMiddleware>& middleware
    ) {
        auto re = PathRegExp(path, paramKeys, canHandlePartial);

        if (method == HttpMethod::ALL) {
            for (const auto m : ALL_HTTP_METHODS) {
                fillConfig(m, re, middleware);
            }
        } else
            fillConfig(method, re, middleware);

        if (middleware->isErrorHandling()) {
            errorHandling = true;
        }
    }

    void Router::fillConfig(
        HttpMethod method,
        const PathRegExp& re,
        std::shared_ptr<AbstractMiddleware> middleware
    ) {
        if (!methodToConfigs[method]) {
            methodToConfigs[method] = std::make_shared<std::vector<MiddlewareConfig>>();
        }
        methodToConfigs[method]->push_back(std::make_pair(re, std::move(middleware)));
    }

    void Router::emit(
        std::shared_ptr<Request> req,
        std::shared_ptr<Response> res,
        NextObject& next
    ) {
        auto middlewareList = methodToConfigs[req->getHttpMethod()];
        auto pipeline = new Pipeline(
            std::move(req),
            std::move(res),
            middlewareList,
            &next,
            isolate
        );

        pipeline->execute();
    }

    // Pipeline

    Pipeline::Pipeline(
        std::shared_ptr<Request> request,
        std::shared_ptr<Response> response,
        std::shared_ptr<std::vector<MiddlewareConfig>> middlewareList,
        v8::Isolate* isolate
    ): req(std::move(request)),
        res(std::move(response)),
        middlewareList(std::move(middlewareList)),
        isBase(true),
        isolate(isolate)
    {
        setNextObject();
    }

    Pipeline::Pipeline(
        std::shared_ptr<Request> request,
        std::shared_ptr<Response> response,
        std::shared_ptr<std::vector<MiddlewareConfig>> middlewareList,
        NextObject* baseNextObj,
        v8::Isolate* isolate
    ): req(std::move(request)),
       res(std::move(response)),
       baseNext(baseNextObj),
       middlewareList(std::move(middlewareList)),
       isBase(false),
       isolate(isolate)
    {
        setNextObject();
    }


    void Pipeline::setNextObject() {
        std::function<void()> nextFunc = [this]() {
            if (!isValid()) {
                res->end();
                return;
            }
            auto nextMiddleware = getNext();

            if (!nextMiddleware) {
                if (!isBase) {
                    res->pipelineEndCallback = baseEliminateCallback;
                    baseNext->next();

                    eliminate();
                } else {
                    if (res->areHeadersSent()) {
                        res->end();
                    }
                    else {
                        res->setStatus(404);
                        res->end();
                    }
                }

                return;
            }
            if (!isHandled) {
                isHandled = true;
                res->setStatus(200);
            }

            req->error.clear();
            nextMiddleware->emit(req, res, next);
        };

        std::function<void()> nextRoute = [this]() {
            if (!isValid()) {
                res->end();
                return;
            }

            auto nextMiddleware = getNext(true);

            if (!nextMiddleware) {
                if (!isBase) {
                    res->pipelineEndCallback = baseEliminateCallback;
                    baseNext->nextRoute();
                    eliminate();
                } else {
                    if (res->areHeadersSent())
                        res->end();
                    else {
                        res->setStatus(404);
                        res->end();
                    }
                }

                return;
            }

            req->error.clear();
            nextMiddleware->emit(req, res, next);
        };

        std::function<void(const std::string&)> error = [this](const std::string& err) {
            if (!isValid()) {
                res->end();
                return;
            }

            req->onDataCallback = nullptr;
            req->onDataEndCallback = nullptr;
            req->error = err;

            auto nextMiddleware = getNextErrorHandling();

            if (!nextMiddleware) {
                if (!isBase) {
                    res->pipelineEndCallback = baseEliminateCallback;
                    baseNext->next();
                    eliminate();
                } else {
                    if (res->areHeadersSent())
                        res->end();
                    else {
                        res->setStatus(500);
                        res->end();
                    }
                }

                return;
            }

            nextMiddleware->emit(req, res, next);
        };

        eliminateCallback = [this]() {
            if (baseEliminateCallback) {
                (*baseEliminateCallback)();
            }

            eliminate();
        };

        baseEliminateCallback = res->pipelineEndCallback;
        res->pipelineEndCallback = &eliminateCallback;

        next = NextObject(nextFunc, nextRoute, error, isolate);
    }

    void Pipeline::execute() {
        next();
    }

    void Pipeline::eliminate() {
        delete this;
    }

    std::shared_ptr<AbstractMiddleware> Pipeline::getNext(bool passCurrentMiddlewares) {
        bool changed = false;

        for(; current < middlewareList->size(); ++current) {
            auto& re = (*middlewareList)[current].first;

            if (!changed && passCurrentMiddlewares && current > 0) {
                auto& previousRe = (*middlewareList)[current - 1].first;

                if (re == previousRe) {
                    continue;
                }
            }

            changed = true;

            auto& path = isBase ? req->pathWithoutQueryString : req->relativePath;

            if (re.match(path, req->routeParams, req->basePath, req->relativePath)) {
                break;
            }
        }

        if (current >= middlewareList->size()) {
            return std::shared_ptr<AbstractMiddleware>{nullptr};
        }

        return (*middlewareList)[current++].second;
    }

    std::shared_ptr<AbstractMiddleware> Pipeline::getNextErrorHandling() {
        std::shared_ptr<AbstractMiddleware> nextMiddleware{nullptr};

        while ((nextMiddleware = getNext())) {
            if (nextMiddleware->isErrorHandling())
                break;
        }

        return nextMiddleware;
    }

    bool Pipeline::isValid() const {
        return req->isAlive && res->isAlive;
    }

    // RouterMethods

    void RouterMethods::Use(const v8::FunctionCallbackInfo<v8::Value> &args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<RouterMethods>(args.Holder());

        if (!wrapInstance || !wrapInstance->routerInstance || args.Length() < 5) {
            return;
        }
        auto instance = wrapInstance->routerInstance;

        HttpMethod method;
        bool canHandlePartial;
        std::string path;
        std::vector<std::string> pathParamKeys;

        for (int i = 0; i < 5; ++i) {
            if (args[i].IsEmpty()) {
                return;
            }
        }

        // Method
        if (args[0]->IsNull())
            method = HttpMethod::ALL;
        else if (args[0]->IsString()) {
            v8::String::Utf8Value internalVal(isolate, args[0]);

            try {
                method = parseMethod(std::string(*internalVal));
            } catch (const std::exception& e) {
                return;
            }
        } else {
            return;
        }

        // CanHandlePartial
        if (args[1]->IsBoolean()) {
            canHandlePartial = args[1].As<v8::Boolean>()->Value();
        } else {
            return;
        }

        // Path Regexp
        if (args[2]->IsNull()) {
            path = "";
        } else if (args[2]->IsString()) {
            v8::String::Utf8Value internalVal(isolate, args[2]);
            path = *internalVal;
        } else {
            return;
        }

        // Path param keys
        if (args[3]->IsArray()) {
            auto paramKeys = v8::Local<v8::Array>::Cast(args[3]);

            if (!paramKeys.IsEmpty() && paramKeys->Length() > 0) {
                pathParamKeys.resize(paramKeys->Length());

                for (size_t i = 0; i < pathParamKeys.size(); ++i) {
                    auto value = paramKeys->Get(i);

                    if (!value->IsString()) {
                        continue;
                    }

                    v8::String::Utf8Value internalParamKey(isolate, value);
                    std::string paramKey(*internalParamKey);

                    pathParamKeys[i] = paramKey;
                }
            }
        } else if (!args[3]->IsNull()) {
            return;
        }

        auto errorHandlingKey = v8::String::NewFromUtf8(isolate, "isErrorHandling",
            v8::NewStringType::kInternalized).ToLocalChecked();

        for (int i = 4; i < args.Length(); ++i) {
            if (args[i].IsEmpty() || (!args[i]->IsObject() && !args[i]->IsFunction())) {
                continue;
            }

            if (args[i]->IsFunction()) {
                bool isErrorHandling = false;
                auto plainMwValue = v8::Local<v8::Function>::Cast(args[i]);
                auto isErrorHandlingInternal = plainMwValue->Get(errorHandlingKey);

                if (!isErrorHandlingInternal.IsEmpty() && isErrorHandlingInternal->IsBoolean()) {
                    isErrorHandling = isErrorHandlingInternal.As<v8::Boolean>()->Value();
                }

                v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> plainMw(isolate, plainMwValue);
                auto plainMiddlewareInstance = std::shared_ptr<PlainMiddleware>(
                        new PlainMiddleware(plainMwValue, isErrorHandling, isolate)
                );

                instance->use(method, canHandlePartial, pathParamKeys, path, plainMiddlewareInstance);
                continue;
            }

            if (args[i]->IsObject()) {
                auto nativeMiddlewareFlag = v8::String::NewFromUtf8(isolate, "__isNexpressNativeMiddleware",
                    v8::NewStringType::kInternalized).ToLocalChecked();
                auto middlewareObject = args[i].As<v8::Object>();

                if (middlewareObject.IsEmpty()) {
                    continue;
                }

                auto nativeFlag = middlewareObject->Get(nativeMiddlewareFlag);

                if (!nativeFlag.IsEmpty() && nativeFlag->IsBoolean() && nativeFlag.As<v8::Boolean>()->Value()) {
                    auto nativeWrapInstance = ObjectWrap::Unwrap<NativeLoadedMiddlewareWrapper>(middlewareObject);

                    instance->use(
                        method,
                        canHandlePartial,
                        pathParamKeys,
                        path,
                        nativeWrapInstance->getInstance()
                    );
                    continue;
                }

                auto internalInstanceKey = v8::String::NewFromUtf8(isolate, "__instance",
                    v8::NewStringType::kInternalized).ToLocalChecked();
                auto appFlag = v8::String::NewFromUtf8(isolate, "__isNexpressApp",
                    v8::NewStringType::kInternalized).ToLocalChecked();
                auto routerFlag = v8::String::NewFromUtf8(isolate, "__isNexpressRouter",
                    v8::NewStringType::kInternalized).ToLocalChecked();

                auto internalInstance = middlewareObject->Get(internalInstanceKey);

                if (internalInstance.IsEmpty() || !internalInstance->IsObject()) {
                    continue;
                }

                auto internalInstanceObject = internalInstance.As<v8::Object>();
                auto appInstanceFlag = middlewareObject->Get(appFlag);
                auto routerInstanceFlag = middlewareObject->Get(routerFlag);

                if (
                    (
                        !appInstanceFlag.IsEmpty()
                        && appInstanceFlag->IsBoolean()
                        && appInstanceFlag.As<v8::Boolean>()->Value()
                    ) || (
                        !routerInstanceFlag.IsEmpty()
                        && routerInstanceFlag->IsBoolean()
                        && routerInstanceFlag.As<v8::Boolean>()->Value()
                    )
                ) {
                    auto routerMethodsInstance = ObjectWrap::Unwrap<RouterMethods>(internalInstanceObject);
                    auto routerInstance = routerMethodsInstance->routerInstance;

                    if (routerInstance) {
                        instance->use(method, canHandlePartial, pathParamKeys, path, routerInstance);
                        continue;
                    }
                }
            }
        }
    }

    v8::Global<v8::Function> RouterWrap::constructor;

    void RouterWrap::Init(v8::Isolate *isolate) {
        Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
        tpl->SetClassName(v8::String::NewFromUtf8(
                isolate, "NexpressRouter", v8::NewStringType::kNormal).ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(tpl, "use", Use);

        Local<v8::Context> context = isolate->GetCurrentContext();
        constructor.Reset(isolate, tpl->GetFunction(context).ToLocalChecked());

        node::AddEnvironmentCleanupHook(isolate, [](void*) {
            constructor.Reset();
        }, nullptr);
    }

    void RouterWrap::NewInstance(const v8::FunctionCallbackInfo<v8::Value> &args) {
        v8::Isolate* isolate = args.GetIsolate();

        v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Object> instance = cons->NewInstance(context).ToLocalChecked();

        args.GetReturnValue().Set(instance);
    }

    void RouterWrap::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        if (args.IsConstructCall()) {
            auto wrapper = new RouterWrap;
            wrapper->routerInstance = std::shared_ptr<Router>(new Router(isolate));

            wrapper->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
        } else {
            v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
            v8::Local<v8::Object> instance = cons->NewInstance(context).ToLocalChecked();
            args.GetReturnValue().Set(instance);
        }
    }
}
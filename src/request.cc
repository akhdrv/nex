#include "request.h"

namespace nex {
    Request::Request(
        std::shared_ptr<HttpConnection> httpConnection,
        v8::Isolate* isolate,
        HttpMethod method,
        std::string path,
        uint32_t minorVersion
    ): connection(std::move(httpConnection)),
        method(method),
        path(std::move(path)),
        minorVersion(minorVersion),
        isolate(isolate)
    {
        prepare();
    }

    void Request::onData(DataReceivedCallback cb) {
        onDataCallback = cb;

        if (!bodyBuffer.empty()) {
            onDataCallback(bodyBuffer);
            bodyBuffer.clear();

            if (isFullData && onDataEndCallback) {
                onDataEndCallback();
            }
        }
    }

    void Request::onDataEnd(DataEndCallback cb) {
        onDataEndCallback = cb;

        if (isFullData && bodyBuffer.empty()) {
            onDataEndCallback();
        }
    }

    void Request::handleData(const std::string& data) {
        if (!isAlive)
            return;

        bodyOctetsReceived += data.length();

        if (bodyOctetsReceived >= contentLength) {
            isFullData = true;
        }

        if (onDataCallback) {
            onDataCallback(data);

            if (isFullData && onDataEndCallback) {
                onDataEndCallback();
            }
        } else {
            bodyBuffer += data;
        }
    }

    void Request::invalidate() {
        isAlive = false;

        if (onDataEndCallback) {
            onDataEndCallback();
        }

        onDataCallback = nullptr;
        onDataEndCallback = nullptr;

        if (jsObj)
            jsObj->invalidate();
    }

    void Request::handleDataEnd() {
        if (!isAlive)
            return;

        if (onDataEndCallback)
            onDataEndCallback();

        onDataEndCallback = nullptr;
        onDataCallback = nullptr;
    }

    void Request::parseQueryString() {
        if (queryString.empty()) {
            return;
        }

        std::stringstream stream(queryString);
        std::string nameBuf, valueBuf;
        stream.get();

        while (!stream.eof()) {
            std::getline(stream, nameBuf, '=');
            std::getline(stream, valueBuf, '&');

            appendQueryParam(nameBuf, valueBuf);
            nameBuf.clear(); valueBuf.clear();
        }
    }

    void Request::appendQueryParam(const std::string& name, std::string value) {
        auto& currentValue = queryParams[name];

        if (auto ref = std::get_if<std::vector<std::string>>(&currentValue)) {
            ref->push_back(value);
            return;
        }

        if (auto ref = std::get_if<std::string>(&currentValue)) {
            auto val = *ref;

            currentValue = std::vector<std::string>{val, value};
            return;
        }

        currentValue = std::move(value);
    }

    void Request::parseCookies() {
        auto valuePtr = std::get_if<std::string>(&getHeader("Cookie"));

        if (!valuePtr || !valuePtr->length()) {
            return;
        }
        std::stringstream stream(*valuePtr);
        std::string nameBuf, valueBuf;

        while (stream) {
            std::getline(stream, nameBuf, '=');
            std::getline(stream, valueBuf, ';');

            if (stream) {
                stream.get();
            }

            cookies[nameBuf] = valueBuf;
            nameBuf.clear(); valueBuf.clear();
        }
    }

    HttpMethod Request::getHttpMethod() const noexcept {
        return method;
    }

    const HeaderValue& Request::getHeader(const std::string& name) {
        return headers[name];
    }

    std::string Request::getHost() {
        if (!host.empty()) {
            return host;
        }

        if (auto ref = std::get_if<std::string>(&getHeader("Host"))) {
            host = *ref;

            return host;
        }

        return std::string();
    }

    std::string Request::getUrl() {
        if (!isAlive) {
            return std::string();
        }
        auto protocol = connection->config->protocol;

        return protocol + "://" + getHost() + path;
    }

    const std::string& Request::getPath() const noexcept {
        return pathWithoutQueryString;
    }

    const std::string& Request::getError() const noexcept {
        return error;
    }

    const std::string& Request::getRelativePath() const noexcept {
        return relativePath;
    }

    const std::string& Request::getQueryString() const noexcept {
        return queryString;
    }

    const QueryParameterValue& Request::getQueryParam(const std::string& name) {
        if (!isQueryStringParsed) {
            isQueryStringParsed = true;
            parseQueryString();
        }

        return queryParams[name];
    }

    const RouteParameterValue& Request::getRouteParam(const std::string& name) {
        return routeParams[name];
    }

    const CookieValue& Request::getCookie(const std::string& name) {
        if (!areCookiesParsed) {
            areCookiesParsed = true;
            parseCookies();
        }

        return cookies[name];
    }

    v8::Persistent<v8::Object>& Request::getJsObject() {
        if (!isAlive) {
            throw std::exception();
        }

        if (!jsObj) {
            createJsObject();
        }

        return jsObj->persistent();
    }

    const CustomDataValue& Request::getCustomData(const std::string& key) {
        return customData[key];
    }

    void Request::setCustomData(const std::string& key, const std::string& value) {
        customData[key] = value;

        if (!jsObj) {
            createJsObject();
        }

        jsObj->setCustomDataToJsObj(key, value);
    }

    Request::~Request() {
        if (jsObj) {
            jsObj->invalidate();
        }
    }

    Request::Request(uint32_t errorStatusCode)
        : requestError(errorStatusCode) {}

    void Request::appendHeader(const std::string& name, std::string value) {
        auto& currentValue = headers[name];

        if (auto ref = std::get_if<std::vector<std::string>>(&currentValue)) {
            ref->push_back(std::move(value));
            return;
        }

        if (auto ref = std::get_if<std::string>(&currentValue)) {
            auto val = *ref;

            currentValue = std::vector<std::string>{val, value};
            return;
        }

        currentValue = std::move(value);
    }

    void Request::setRelativePath(const std::string& relPath) {
        relativePath = relPath;
    }

    void Request::setRouteParameter(const std::string& name, RouteParameterValue value) {
        routeParams[name] = value;
    }

    void Request::clearRouteParameters() {
        routeParams.clear();
    }

    void Request::prepare() {
        uint32_t queryStringStart = 0, anchorStart = 0;
        bool isAnchor = false;

        for (; queryStringStart < path.length(); ++queryStringStart) {
            if (path[queryStringStart] == '?')
                break;
            else if (path[anchorStart] == '#') {
                isAnchor = true;
                break;
            }
        }

        if (queryStringStart >= path.length()) {
            pathWithoutQueryString = path;
            return;
        }

        pathWithoutQueryString = std::string(path.begin(), path.begin() + queryStringStart);

        if (isAnchor) {
            anchor = std::string(path.begin() + anchorStart, path.end());
            return;
        }

        for (; anchorStart < path.length(); ++anchorStart) {
            if (path[anchorStart] == '#') {
                break;
            }
        }

        if (anchorStart >= path.length()) {
            queryString = std::string(path.begin() + queryStringStart, path.end());
            return;
        }

        queryString = std::string(path.begin() + queryStringStart, path.begin() + anchorStart);
        anchor = std::string(path.begin() + anchorStart, path.end());
    }

    void Request::createJsObject() {
        if (!jsObj) {
            jsObj = RequestWrap::NewInstance(isolate, this);
        }
    }

    // RequestWrap

    v8::Global<v8::Function> RequestWrap::constructor;

    void RequestWrap::invalidate() {
        if (!isValid) {
            return;
        }

        isValid = false;
        Unref();
    }

    void RequestWrap::setCustomDataToJsObj(const std::string& key, const std::string& data) {
        if (!isValid) {
            return;
        }

        auto isolate = instance->isolate;
        auto inst = handle(isolate);
        auto customDataObjectKey = v8::String::NewFromUtf8(
                isolate, "customData", v8::NewStringType::kNormal).ToLocalChecked();
        auto customDataKey = v8::String::NewFromUtf8(
                isolate, key.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
        auto customDataValue = v8::String::NewFromUtf8(
                isolate, data.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
        auto customDataRaw = inst->Get(customDataObjectKey);

        if (!customDataRaw->IsObject()) {
            customDataRaw = v8::Object::New(isolate);

            inst->Set(customDataObjectKey, customDataRaw);
        }

        auto customData = customDataRaw.As<v8::Object>();

        customData->Set(customDataKey, customDataValue);
    }

    void RequestWrap::setFields() {
        if (!isValid) {
            return;
        }
        auto isolate = instance->isolate;
        auto reqObjectHandle = handle(isolate);

        /// Cookie

        auto cookieKey = v8::String::NewFromUtf8(
                isolate, "cookies", v8::NewStringType::kNormal).ToLocalChecked();
        auto cookieObj = v8::Object::New(isolate);
        reqObjectHandle->Set(cookieKey, cookieObj);

        for (const auto& [key, value] : instance->cookies) {
            if (auto ref = std::get_if<std::string>(&value)) {
                auto k = v8::String::NewFromUtf8(
                        isolate, key.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
                auto v = v8::String::NewFromUtf8(
                        isolate, ref->c_str(), v8::NewStringType::kNormal).ToLocalChecked();

                cookieObj->Set(k, v);
            }
        }

        /// Route Params

        auto routeParamsKey = v8::String::NewFromUtf8(
                isolate, "params", v8::NewStringType::kNormal).ToLocalChecked();
        auto routeParamsObj = v8::Object::New(isolate);
        reqObjectHandle->Set(routeParamsKey, routeParamsObj);

        for (const auto& [key, value] : instance->routeParams) {
            if (auto ref = std::get_if<std::string>(&value)) {
                auto k = v8::String::NewFromUtf8(
                        isolate, key.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
                auto v = v8::String::NewFromUtf8(
                        isolate, ref->c_str(), v8::NewStringType::kNormal).ToLocalChecked();

                routeParamsObj->Set(k, v);
            }
        }

        /// Query Params

        auto queryParamsKey = v8::String::NewFromUtf8(
                isolate, "query", v8::NewStringType::kNormal).ToLocalChecked();
        auto queryParamsObj = v8::Object::New(isolate);
        reqObjectHandle->Set(queryParamsKey, queryParamsObj);

        for (const auto& [key, value] : instance->queryParams) {
            auto k = v8::String::NewFromUtf8(
                    isolate, key.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
            v8::Local<v8::Value> v;

            if (auto ref = std::get_if<std::string>(&value)) {
                v = v8::String::NewFromUtf8(
                        isolate, ref->c_str(), v8::NewStringType::kNormal).ToLocalChecked();
            }

            if (auto ref = std::get_if<std::vector<std::string>>(&value)) {
                auto values = v8::Array::New(isolate, ref->size());

                if (!values.IsEmpty()) {
                    for (size_t i = 0; i < ref->size(); ++i) {
                        auto value = v8::String::NewFromUtf8(
                                isolate, (*ref)[i].c_str(), v8::NewStringType::kNormal).ToLocalChecked();
                        values->Set(i, value);
                    }

                    v = values;
                }
            }

            if (!v.IsEmpty()) {
                queryParamsObj->Set(k, v);
            }
        }

        /// Hostname

        auto hostNameKey = v8::String::NewFromUtf8(
                isolate, "hostname", v8::NewStringType::kNormal).ToLocalChecked();
        auto hostNameValue = v8::String::NewFromUtf8(
                isolate, instance->getHost().c_str(), v8::NewStringType::kNormal).ToLocalChecked();

        reqObjectHandle->Set(hostNameKey, hostNameValue);

        /// Method

        auto methodKey = v8::String::NewFromUtf8(
                isolate, "method", v8::NewStringType::kNormal).ToLocalChecked();
        auto methodValue = v8::String::NewFromUtf8(
                isolate, methodToString(instance->method).c_str(), v8::NewStringType::kNormal)
                        .ToLocalChecked();

        reqObjectHandle->Set(methodKey, methodValue);

        /// originalUrl

        auto originalUrlKey = v8::String::NewFromUtf8(
                isolate, "originalUrl", v8::NewStringType::kNormal).ToLocalChecked();
        auto originalUrlValue = v8::String::NewFromUtf8(
                isolate, instance->path.c_str(), v8::NewStringType::kNormal).ToLocalChecked();

        reqObjectHandle->Set(originalUrlKey, originalUrlValue);

        /// Relative Path

        auto relativePathKey = v8::String::NewFromUtf8(
                isolate, "path", v8::NewStringType::kNormal).ToLocalChecked();
        auto relativePathValue = v8::String::NewFromUtf8(
                isolate, instance->getRelativePath().c_str(), v8::NewStringType::kNormal).ToLocalChecked();

        reqObjectHandle->Set(relativePathKey, relativePathValue);
    }

    void RequestWrap::Init(v8::Isolate* isolate) {
        v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, New);

        tpl->SetClassName(v8::String::NewFromUtf8(
                isolate, "NRequest", v8::NewStringType::kNormal).ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(tpl, "on", On);
        NODE_SET_PROTOTYPE_METHOD(tpl, "get", Get);
        NODE_SET_PROTOTYPE_METHOD(tpl, "setRequestCustomData", SetCustomData);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        constructor.Reset(isolate, tpl->GetFunction(context).ToLocalChecked());

        node::AddEnvironmentCleanupHook(isolate, [](void*) {
            constructor.Reset();
        }, nullptr);
    }

    RequestWrap* RequestWrap::NewInstance(v8::Isolate* isolate, Request* instance) {
        v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Object> inst = cons->NewInstance(context).ToLocalChecked();

        auto wrapInstance = ObjectWrap::Unwrap<RequestWrap>(inst);
        wrapInstance->instance = instance;
        wrapInstance->setFields();
        wrapInstance->Ref();

        return wrapInstance;
    }

    void RequestWrap::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        if (args.IsConstructCall()) {
            auto app = new RequestWrap;

            app->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
        } else {
            v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
            v8::Local<v8::Object> instance =
                    cons->NewInstance(context).ToLocalChecked();
            args.GetReturnValue().Set(instance);
        }
    }

    void RequestWrap::Get(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<RequestWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString()) {
            return;
        }

        v8::String::Utf8Value internalHeaderName(isolate, args[0]);
        std::string headerName(*internalHeaderName);

        auto header = wrapInstance->instance->getHeader(headerName);

        if (auto ref = std::get_if<std::string>(&header)) {
            auto str = v8::String::NewFromUtf8(
                    isolate, ref->c_str(), v8::NewStringType::kNormal).ToLocalChecked();

            args.GetReturnValue().Set(str);
            return;
        }

        if (auto ref = std::get_if<std::vector<std::string>>(&header)) {
            v8::Local<v8::Array> headerValues = v8::Array::New(isolate, ref->size());

            if (headerValues.IsEmpty())
                return;

            for (size_t i = 0; i < ref->size(); ++i) {
                auto value = v8::String::NewFromUtf8(
                        isolate, (*ref)[i].c_str(), v8::NewStringType::kNormal).ToLocalChecked();
                headerValues->Set(i, value);
            }

            args.GetReturnValue().Set(headerValues);
            return;
        }
    }

    void RequestWrap::On(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<RequestWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString() || !args[1]->IsFunction()) {
            return;
        }

        v8::String::Utf8Value internalEventName(isolate, args[0]);
        std::string eventName(*internalEventName);
        auto localCallback = v8::Local<v8::Function>::Cast(args[1]);
        v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> callback(isolate, localCallback);

        if (eventName == "data") {
            wrapInstance->onDataCallback = callback;
            wrapInstance->instance->onData([instance = wrapInstance](const std::string& data) {
                if (!instance->isValid) {
                    return;
                }
                auto isolate = instance->instance->isolate;

                if (!instance->onDataCallback.IsEmpty()) {
                    v8::HandleScope handleScope(isolate);

                    auto str = v8::String::NewFromUtf8(
                            isolate, data.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
                    auto context = isolate->GetCurrentContext();
                    v8::Local<v8::Value> argv[] = {str};

                    instance->onDataCallback.Get(isolate)->Call(context, v8::Null(isolate), 1, argv);
                }
            });

            return;
        }

        if (eventName == "end") {
            wrapInstance->onDataEndCallback = callback;
            wrapInstance->instance->onDataEnd([instance = wrapInstance]() {
                if (!instance->isValid) {
                    return;
                }
                auto isolate = instance->instance->isolate;

                if (!instance->onDataEndCallback.IsEmpty()) {
                    v8::HandleScope handleScope(isolate);

                    auto context = isolate->GetCurrentContext();
                    v8::Local<v8::Value> argv[0];

                    instance->onDataEndCallback.Get(isolate)->Call(context, v8::Null(isolate), 0, argv);
                }
            });

            return;
        }
    }

    void RequestWrap::SetCustomData(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<RequestWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString() || !args[1]->IsString()) {
            return;
        }

        v8::String::Utf8Value internalKey(isolate, args[0]),
            internalValue(isolate, args[1]);
        std::string key(*internalKey), value(*internalValue);

        wrapInstance->instance->customData[key] = value;
    }

    RequestWrap::~RequestWrap() {
        if (isValid) {
            instance->jsObj = nullptr;
        }
    }

}
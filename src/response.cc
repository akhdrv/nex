#include "response.h"

namespace nex {

    // Response

    Response::Response(
        std::shared_ptr<HttpConnection> httpConnection,
        v8::Isolate* isolate,
        uint32_t httpMinorVersion
    ) : httpConnection(std::move(httpConnection)),
        minorVersion(httpMinorVersion),
        isolate(isolate)
    {
        setBasicHeaders();
    }

    void Response::invalidate() {
        isAlive = false;

        if (jsObj) {
            jsObj->invalidate();
        }
    }

    void Response::end() {
        if (!isAlive) {
            // recursively cleanup pipelines memory
            if (pipelineEndCallback) {
                (*pipelineEndCallback)();
                pipelineEndCallback = nullptr;
            }
            return;
        }

        if (!headersSent) {
            contentLength = 0;
            sendHeaders();
        }

        if (headersSent && isChunkedTransfer) {
            writeInternal("");
        }

        invalidate();

        // recursively cleanup pipelines memory
        if (pipelineEndCallback) {
            (*pipelineEndCallback)();
            pipelineEndCallback = nullptr;
        }

        httpConnection->end();
    }

    void Response::send(const std::string& data) {
        if (!isAlive) {
            return;
        }

        if (!headersSent) {
            contentLength = data.length();
            sendHeaders();
        }

        writeInternal(data);

        end();
    }

    void Response::sendHeaders() {
        if (!isAlive || headersSent) {
            return;
        }

        updateHeadersBeforeSending();

        std::stringstream buffer;

        buffer << "HTTP/1." << minorVersion << " " << statusCode << " "
               << getStatusTextByCode(statusCode) << CRLF;

        for (const auto& [key, value]: headers) {
            if (auto ref = std::get_if<std::string>(&value)) {
                buffer << key << ": " << *ref << CRLF;
                continue;
            }

            if (auto ref = std::get_if<std::vector<std::string>>(&value)) {
                for (auto& v : *ref) {
                    buffer << key << ": " << v << CRLF;
                }
                continue;
            }
        }

        buffer << CRLF;

        writeRaw(buffer.str());
        headersSent = true;
    }

    void Response::setStatus(uint32_t code) {
        statusCode = code;
    }

    void Response::setHeader(const std::string& name, HeaderValue value) {
        headers[name] = value;
    }

    bool Response::areHeadersSent() const {
        return headersSent;
    }

    void Response::writeRaw(const std::string& data) {
        auto size = data.length();
        auto dataPtr = std::make_unique<char[]>(size + 1);
        std::copy(data.begin(), data.end(), dataPtr.get());
        dataPtr[size] = '\0';

        httpConnection->client->write(std::move(dataPtr), size);
    }

    void Response::writeInternal(const std::string& data) {
        if (!isChunkedTransfer) {
            writeRaw(data);
            return;
        }

        std::stringstream buffer;

        buffer << std::hex << data.length() << CRLF
            << data << CRLF;

        writeRaw(buffer.str());
    }

    const HeaderValue& Response::getHeader(const std::string& name) {
        return headers[name];
    }

    const ResponseCookieValue& Response::getCookie(const std::string& name) {
        return cookies[name];
    }

    void Response::appendHeader(const std::string& name, std::string value) {
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

    void Response::setCookie(const std::string& name, ResponseCookieValue value) {
        cookies[name] = std::move(value);
    }

    void Response::clearCookie(const std::string& name, ResponseCookieValue value) {
        value.maxAge = 0;
        value.expires = 0;

        cookies[name] = std::move(value);
    }

    void Response::write(const std::string& data) {
        if (!isAlive) {
            return;
        }

        if (!headersSent) {
            if (httpConnection->config->persistentConnections)
                isChunkedTransfer = true;
            sendHeaders();
        }

        writeInternal(data);
    }

    void Response::sendStatus(uint32_t code) {
        if (!isAlive) {
            return;
        }

        if (!headersSent) {
            sendHeaders();
        }
    }

    Response::~Response() {
        if (jsObj) {
            jsObj->invalidate();
        }
    }

    void Response::setBasicHeaders() {
        if (!isAlive) {
            return;
        }

        headers["Date"] = getStandardizedTime();
        headers["Content-Type"] = "text/plain; charset=utf-8";

        if (httpConnection->config->persistentConnections) {
            headers["Connection"] = "keep-alive";
            headers["Keep-Alive"] = "timeout=" +
                    std::to_string(httpConnection->config->keepAliveTimeout / 1000) +
                    ", max=" + std::to_string(httpConnection->config->maxRequestsPerConnection);
        } else {
            headers["Connection"] = "close";
        }
    }

    void Response::updateHeadersBeforeSending() {
        if (!isAlive) {
            return;
        }

        if (isChunkedTransfer) {
            headers["Transfer-Encoding"] = "chunked";
        } else if (auto length = std::get_if<uint32_t>(&contentLength)) {
            headers["Content-Length"] = std::to_string(*length);
        } else {
            headers["Transfer-Encoding"] = "identity";
        }

        if (auto contentType = std::get_if<std::string>(&headers["Content-Type"])) {
            if (contentType->find("charset") == std::string::npos) {
                headers["Content-Type"] = *contentType + "; charset=utf-8";
            }
        }

        if (!cookies.empty()) {
            std::vector<std::string> values(cookies.size());
            size_t i = 0;

            for (auto& [name, cookieValue] : cookies) {
                values[i++] = ResponseCookieValue::serialize(name, cookieValue);
            }

            headers["Set-Cookie"] = values;
        }
    }

    void Response::createJsObject() {
        if (!jsObj) {
            jsObj = ResponseWrap::NewInstance(isolate, this);
        }
    }

    v8::Persistent<v8::Object>& Response::getJsObject() {
        if (!isAlive) {
            throw std::exception();
        }

        if (!jsObj) {
            createJsObject();
        }

        return jsObj->persistent();
    }


    // ResponseWrap

    v8::Global<v8::Function> ResponseWrap::constructor;

    void ResponseWrap::invalidate() {
        if (!isValid) {
            return;
        }

        isValid = false;
        Unref();
    }

    void ResponseWrap::updateFields() {
        if (!isValid) {
            return;
        }

        auto isolate = instance->isolate;
        auto reqObjectHandle = handle(isolate);

        auto headersSentKey = v8::String::NewFromUtf8(
                isolate, "headersSent", v8::NewStringType::kNormal).ToLocalChecked();
        auto headersSentValue = v8::Boolean::New(isolate, instance->areHeadersSent());

        reqObjectHandle->Set(headersSentKey, headersSentValue);
    }

    void ResponseWrap::Init(v8::Isolate* isolate) {
        v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, New);

        tpl->SetClassName(v8::String::NewFromUtf8(
                isolate, "NResponse", v8::NewStringType::kNormal).ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        NODE_SET_PROTOTYPE_METHOD(tpl, "cookie", SetCookie);
        NODE_SET_PROTOTYPE_METHOD(tpl, "clearCookie", ClearCookie);
        NODE_SET_PROTOTYPE_METHOD(tpl, "end", End);
        NODE_SET_PROTOTYPE_METHOD(tpl, "get", GetHeader);
        NODE_SET_PROTOTYPE_METHOD(tpl, "set", SetHeader);
        NODE_SET_PROTOTYPE_METHOD(tpl, "send", Send);
        NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);
        NODE_SET_PROTOTYPE_METHOD(tpl, "sendStatus", SendStatus);
        NODE_SET_PROTOTYPE_METHOD(tpl, "status", SetStatus);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        constructor.Reset(isolate, tpl->GetFunction(context).ToLocalChecked());

        node::AddEnvironmentCleanupHook(isolate, [](void*) {
            constructor.Reset();
        }, nullptr);
    }

    ResponseWrap* ResponseWrap::NewInstance(v8::Isolate* isolate, Response* instance) {
        v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Object> inst = cons->NewInstance(context).ToLocalChecked();

        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(inst);
        wrapInstance->instance = instance;
        wrapInstance->updateFields();
        wrapInstance->Ref();

        return wrapInstance;
    }

    void ResponseWrap::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        if (args.IsConstructCall()) {
            auto app = new ResponseWrap;

            app->Wrap(args.This());
            args.GetReturnValue().Set(args.This());
        } else {
            v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
            v8::Local<v8::Object> instance =
                    cons->NewInstance(context).ToLocalChecked();
            args.GetReturnValue().Set(instance);
        }
    }

    void ResponseWrap::GetHeader(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

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

    void ResponseWrap::SetHeader(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString() || !(args[1]->IsString() || args[1]->IsArray())) {
            return;
        }

        v8::String::Utf8Value internalHeaderName(isolate, args[0]);
        std::string headerName(*internalHeaderName);

        if (args[1]->IsString()) {
            v8::String::Utf8Value internalHeaderValue(isolate, args[1]);
            std::string headerValue(*internalHeaderValue);

            wrapInstance->instance->setHeader(headerName, headerValue);
        }

        if (args[1]->IsArray()) {
            auto headerValues = v8::Local<v8::Array>::Cast(args[1]);

            if (headerValues.IsEmpty()) {
                return;
            }

            std::vector<std::string> values(headerValues->Length());

            for (size_t i = 0; i < values.size(); ++i) {
                auto value = headerValues->Get(i);

                if (!value->IsString()) {
                    continue;
                }

                v8::String::Utf8Value internalHeaderValue(isolate, value);
                std::string headerValue(*internalHeaderValue);

                values[i] = headerValue;
            }

            wrapInstance->instance->setHeader(headerName, values);
        }
    }

    void ResponseWrap::SetCookie(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString() || !args[1]->IsString()) {
            return;
        }

        v8::String::Utf8Value internalCookieName(isolate, args[0]),
                internalCookieValue(isolate, args[1]);
        std::string cookieName(*internalCookieName), cookieValue(*internalCookieValue);
        ResponseCookieValue cookieObj;

        cookieObj.value = cookieValue;

        if (!args[2]->IsObject()) {
            wrapInstance->instance->setCookie(cookieName, cookieObj);
            return;
        }

        auto cookieOptions = v8::Local<v8::Object>::Cast(args[2]);

        if (cookieOptions.IsEmpty()) {
            wrapInstance->instance->setCookie(cookieName, cookieObj);
            return;
        }

        auto domainKey = v8::String::NewFromUtf8(
                isolate, "domain", v8::NewStringType::kNormal).ToLocalChecked();
        auto expiresKey = v8::String::NewFromUtf8(
                isolate, "expires", v8::NewStringType::kNormal).ToLocalChecked();
        auto httpOnlyKey = v8::String::NewFromUtf8(
                isolate, "httpOnly", v8::NewStringType::kNormal).ToLocalChecked();
        auto maxAgeKey = v8::String::NewFromUtf8(
                isolate, "maxAge", v8::NewStringType::kNormal).ToLocalChecked();
        auto pathKey = v8::String::NewFromUtf8(
                isolate, "path", v8::NewStringType::kNormal).ToLocalChecked();
        auto secureKey = v8::String::NewFromUtf8(
                isolate, "secure", v8::NewStringType::kNormal).ToLocalChecked();
        auto sameSiteKey = v8::String::NewFromUtf8(
                isolate, "sameSite", v8::NewStringType::kNormal).ToLocalChecked();

        auto domain = cookieOptions->Get(domainKey);
        auto expires = cookieOptions->Get(expiresKey);
        auto httpOnly = cookieOptions->Get(httpOnlyKey);
        auto maxAge = cookieOptions->Get(maxAgeKey);
        auto path = cookieOptions->Get(pathKey);
        auto secure = cookieOptions->Get(secureKey);
        auto sameSite = cookieOptions->Get(sameSiteKey);

        if (domain->IsString()) {
            v8::String::Utf8Value internalVal(isolate, domain);

            cookieObj.domain = std::string(*internalVal);
        }

        if (expires->IsDate()) {
            auto date = v8::Local<v8::Date>::Cast(expires);

            if (!date.IsEmpty()) {
                auto value = static_cast<time_t>(date->ValueOf() / 1000);

                cookieObj.expires = value;
            }
        }

        if (httpOnly->IsBoolean()) {
            auto internalVal = v8::Local<v8::Boolean>::Cast(httpOnly);

            if (!internalVal.IsEmpty()) {
                auto val = internalVal->Value();

                cookieObj.httpOnly = val;
            }
        }

        if (maxAge->IsNumber()) {
            auto internalVal = v8::Local<v8::Number>::Cast(maxAge);

            if (!internalVal.IsEmpty()) {
                auto val = static_cast<uint32_t>(internalVal->Value() / 1000);

                cookieObj.maxAge = val;
            }
        }

        if (path->IsString()) {
            v8::String::Utf8Value internalVal(isolate, path);

            cookieObj.path = std::string(*internalVal);
        }

        if (secure->IsBoolean()) {
            auto internalVal = v8::Local<v8::Boolean>::Cast(secure);

            if (!internalVal.IsEmpty()) {
                auto val = internalVal->Value();

                cookieObj.secure = val;
            }
        }

        if (sameSite->IsBoolean()) {
            auto internalVal = v8::Local<v8::Boolean>::Cast(sameSite);

            if (!internalVal.IsEmpty()) {
                auto val = internalVal->Value();

                if (!val) {
                    cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::None;
                }
            }
        } else if (sameSite->IsString()) {
            v8::String::Utf8Value internalVal(isolate, sameSite);
            auto val = std::string(*internalVal);
            stringToLower(val);

            if (val == "lax")
                cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::Lax;
            else if (val == "strict")
                cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::Strict;
            else if (val == "none")
                cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::None;
        }

        wrapInstance->instance->setCookie(cookieName, cookieObj);
    }

    void ResponseWrap::ClearCookie(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString()) {
            return;
        }

        v8::String::Utf8Value internalCookieName(isolate, args[0]);
        std::string cookieName(*internalCookieName);
        ResponseCookieValue cookieObj;

        cookieObj.maxAge = 0;
        cookieObj.expires = 0;

        if (!args[1]->IsObject()) {
            wrapInstance->instance->setCookie(cookieName, cookieObj);
            return;
        }

        auto cookieOptions = v8::Local<v8::Object>::Cast(args[2]);

        if (cookieOptions.IsEmpty()) {
            wrapInstance->instance->setCookie(cookieName, cookieObj);
            return;
        }

        auto domainKey = v8::String::NewFromUtf8(
                isolate, "domain", v8::NewStringType::kNormal).ToLocalChecked();
        auto expiresKey = v8::String::NewFromUtf8(
                isolate, "expires", v8::NewStringType::kNormal).ToLocalChecked();
        auto httpOnlyKey = v8::String::NewFromUtf8(
                isolate, "httpOnly", v8::NewStringType::kNormal).ToLocalChecked();
        auto maxAgeKey = v8::String::NewFromUtf8(
                isolate, "maxAge", v8::NewStringType::kNormal).ToLocalChecked();
        auto pathKey = v8::String::NewFromUtf8(
                isolate, "path", v8::NewStringType::kNormal).ToLocalChecked();
        auto secureKey = v8::String::NewFromUtf8(
                isolate, "secure", v8::NewStringType::kNormal).ToLocalChecked();
        auto sameSiteKey = v8::String::NewFromUtf8(
                isolate, "sameSite", v8::NewStringType::kNormal).ToLocalChecked();

        auto domain = cookieOptions->Get(domainKey);
        auto expires = cookieOptions->Get(expiresKey);
        auto httpOnly = cookieOptions->Get(httpOnlyKey);
        auto maxAge = cookieOptions->Get(maxAgeKey);
        auto path = cookieOptions->Get(pathKey);
        auto secure = cookieOptions->Get(secureKey);
        auto sameSite = cookieOptions->Get(sameSiteKey);

        if (domain->IsString()) {
            v8::String::Utf8Value internalVal(isolate, domain);

            cookieObj.domain = std::string(*internalVal);
        }

        if (expires->IsDate()) {
            auto date = v8::Local<v8::Date>::Cast(expires);

            if (!date.IsEmpty()) {
                auto value = static_cast<time_t>(date->ValueOf() / 1000);

                cookieObj.expires = value;
            }
        }

        if (httpOnly->IsBoolean()) {
            auto internalVal = v8::Local<v8::Boolean>::Cast(httpOnly);

            if (!internalVal.IsEmpty()) {
                auto val = internalVal->Value();

                cookieObj.httpOnly = val;
            }
        }

        if (maxAge->IsNumber()) {
            auto internalVal = v8::Local<v8::Number>::Cast(maxAge);

            if (!internalVal.IsEmpty()) {
                auto val = static_cast<uint32_t>(internalVal->Value() / 1000);

                cookieObj.maxAge = val;
            }
        }

        if (path->IsString()) {
            v8::String::Utf8Value internalVal(isolate, path);

            cookieObj.path = std::string(*internalVal);
        }

        if (secure->IsBoolean()) {
            auto internalVal = v8::Local<v8::Boolean>::Cast(secure);

            if (!internalVal.IsEmpty()) {
                auto val = internalVal->Value();

                cookieObj.secure = val;
            }
        }

        if (sameSite->IsBoolean()) {
            auto internalVal = v8::Local<v8::Boolean>::Cast(sameSite);

            if (!internalVal.IsEmpty()) {
                auto val = internalVal->Value();

                if (!val) {
                    cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::None;
                }
            }
        } else if (sameSite->IsString()) {
            v8::String::Utf8Value internalVal(isolate, sameSite);
            auto val = std::string(*internalVal);
            stringToLower(val);

            if (val == "lax")
                cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::Lax;
            else if (val == "strict")
                cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::Strict;
            else if (val == "none")
                cookieObj.sameSite = ResponseCookieValue::SameSiteAttribute::None;
        }

        wrapInstance->instance->setCookie(cookieName, cookieObj);
    }

    void ResponseWrap::Send(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString()) {
            return;
        }

        v8::String::Utf8Value internalData(isolate, args[0]);
        wrapInstance->instance->send(std::string(*internalData));
    }

    void ResponseWrap::SendStatus(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsNumber()) {
            return;
        }

        auto internalVal = v8::Local<v8::Number>::Cast(args[0]);

        if (!internalVal.IsEmpty()) {
            auto val = static_cast<uint32_t>(internalVal->Value());

            wrapInstance->instance->sendStatus(val);
        }
    }

    void ResponseWrap::Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsString()) {
            return;
        }

        v8::String::Utf8Value internalData(isolate, args[0]);
        wrapInstance->instance->write(std::string(*internalData));
    }

    void ResponseWrap::SetStatus(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate = args.GetIsolate();
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid || !args[0]->IsNumber()) {
            return;
        }

        auto internalVal = v8::Local<v8::Number>::Cast(args[0]);

        if (!internalVal.IsEmpty()) {
            auto val = static_cast<uint32_t>(internalVal->Value());

            wrapInstance->instance->setStatus(val);
        }
    }

    void ResponseWrap::End(const v8::FunctionCallbackInfo<v8::Value>& args) {
        auto wrapInstance = ObjectWrap::Unwrap<ResponseWrap>(args.Holder());

        if (!wrapInstance->isValid) {
            return;
        }

        wrapInstance->instance->end();
    }

    ResponseWrap::~ResponseWrap() {
        if (isValid) {
            instance->jsObj = nullptr;
        }
    }


}

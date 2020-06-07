#pragma once


#include <sstream>
#include <iomanip>
#include <node.h>
#include <node_object_wrap.h>

#include "commonHeaders.h"
#include "httpConnection.h"

namespace nex {

inline const char CRLF[] = "\r\n";

class Router;
class ResponseWrap;
class HttpConnection;

typedef std::map<std::string, ResponseCookieValue> ResponseCookieMapping;

class AbstractResponse {
public:

    [[nodiscard]] virtual const HeaderValue& getHeader(const std::string& name) = 0;
    [[nodiscard]] virtual const ResponseCookieValue& getCookie(const std::string& name) = 0;
    [[nodiscard]] virtual bool areHeadersSent() const = 0;
    [[nodiscard]] virtual v8::Persistent<v8::Object>& getJsObject() = 0;

    virtual void setHeader(const std::string& name, HeaderValue value) = 0;
    virtual void appendHeader(const std::string& name, std::string value) = 0;
    virtual void setCookie(const std::string& name, ResponseCookieValue value) = 0;
    virtual void clearCookie(const std::string& name, ResponseCookieValue value) = 0;
    virtual void write(const std::string& data) = 0;
    virtual void sendStatus(uint32_t code) = 0;
    virtual void setStatus(uint32_t code) = 0;
    virtual void end() = 0;

    virtual void send(const std::string& data) = 0;
};


class Response final : public AbstractResponse {
public:

    [[nodiscard]] const HeaderValue& getHeader(const std::string& name) override;
    [[nodiscard]] const ResponseCookieValue& getCookie(const std::string& name) override;
    [[nodiscard]] bool areHeadersSent() const override;
    [[nodiscard]] v8::Persistent<v8::Object>& getJsObject() override;

    void setHeader(const std::string& name, HeaderValue value) override;
    void appendHeader(const std::string& name, std::string value) override;
    void setCookie(const std::string& name, ResponseCookieValue value) override;
    void clearCookie(const std::string& name, ResponseCookieValue value) override;
    void write(const std::string& data) override;
    void sendStatus(uint32_t code) override;
    void setStatus(uint32_t code) override;
    void end() override;

    void send(const std::string& data) override;

    ~Response();

private:
    friend class HttpConnection;
    friend class Pipeline;
    friend class ResponseWrap;

    Response(
        std::shared_ptr<HttpConnection> httpConnection,
        v8::Isolate* isolate,
        uint32_t minorVersion
    );

    void sendHeaders();

    void invalidate();

    void writeRaw(const std::string& data);
    void writeInternal(const std::string& data);
    void setBasicHeaders();
    void updateHeadersBeforeSending();
    void createJsObject();

    std::shared_ptr<HttpConnection> httpConnection;

    std::function<void()>* pipelineEndCallback = nullptr;

    ResponseCookieMapping cookies;
    HeaderMapping headers;

    uint32_t statusCode = 500;
    uint32_t minorVersion = 1;
    std::variant<std::monostate, uint32_t> contentLength;

    bool isAlive = true;
    bool headersSent = false;
    bool isChunkedTransfer = false;

    v8::Isolate* isolate;
    ResponseWrap* jsObj = nullptr;
};

class ResponseWrap: public node::ObjectWrap {
public:

    void invalidate();
    void updateFields();

    static void Init(v8::Isolate* isolate);
    static ResponseWrap* NewInstance(v8::Isolate* isolate, Response* instance);

    ~ResponseWrap() override;
private:
    explicit ResponseWrap() = default;

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static v8::Global<v8::Function> constructor;

    /**
     * Gets/Sets Header value
     */
    static void GetHeader(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void SetHeader(const v8::FunctionCallbackInfo<v8::Value>& args);

    static void SetCookie(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void ClearCookie(const v8::FunctionCallbackInfo<v8::Value>& args);

    static void Send(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void SendStatus(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void SetStatus(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void End(const v8::FunctionCallbackInfo<v8::Value>& args);

    bool isValid = true;
    Response* instance = nullptr;
};

}

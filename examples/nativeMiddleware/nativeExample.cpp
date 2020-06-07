#include <functional>
#include <memory>
#include <request.h>
#include <response.h>

extern "C" void emit(
    std::shared_ptr<nex::AbstractRequest> req,
    std::shared_ptr<nex::AbstractResponse> res,
    const std::function<void()>& next,
    const std::function<void()>& nextRoute,
    const std::function<void(std::string)>& error
) {
    res->setHeader("X-NATIVE-MIDDLEWARE", "1");
    res->send("I'm a native middleware\n");
}

extern "C" bool isErrorHandling() {
    return false;
}

#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <memory>
#include <exception>
#include <regex>
#include <iostream>
#include <fstream>
#include <variant>
#include <queue>

#include "helpers/methods.h"
#include "helpers/miscellaneous.h"
#include "helpers/statusCodes.h"

namespace nex {
    typedef std::variant<std::monostate, std::string, std::vector<std::string>> MaybeStringArrayValue;
    typedef std::variant<std::monostate, std::string> MaybeStringValue;
    typedef MaybeStringArrayValue HeaderValue;
    typedef MaybeStringArrayValue QueryParameterValue;
    typedef MaybeStringValue RouteParameterValue;
    typedef MaybeStringValue CookieValue;
    typedef MaybeStringValue CustomDataValue;

    typedef std::map<std::string, QueryParameterValue, caseInsensitiveCompare> QueryParamMapping;
    typedef std::map<std::string, HeaderValue, caseInsensitiveCompare> HeaderMapping;
    typedef std::map<std::string, RouteParameterValue, caseInsensitiveCompare> RouteParamMapping;
    typedef std::map<std::string, CookieValue> CookieMapping;
    typedef std::map<std::string, CustomDataValue> CustomDataMapping;
}


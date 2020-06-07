#pragma once

#ifndef PATHREGEXP_H
#define PATHREGEXP_H

#include "commonHeaders.h"

namespace nex {

    class PathRegExp {
    public:

        PathRegExp(std::string regex, std::vector<std::string> paramKeys, bool canHandlePartial);

        bool match(
                const std::string& path,
                RouteParamMapping& mapping,
                std::string& basePath,
                std::string& restPath
        );

        inline bool operator==(const PathRegExp& other) {
            return regexString == other.regexString;
        }

        inline bool operator!=(const PathRegExp& other) {
            return !(*this == other);
        }

    private:
        bool matchWhole(const std::string& path, RouteParamMapping& mapping, std::string& basePath);

        bool matchPartial(
                const std::string& path,
                RouteParamMapping& mapping,
                std::string& basePath,
                std::string& restPath
        );

        inline static std::regex getRegex(const std::string& pattern) {
            return std::regex(pattern, std::regex::ECMAScript | std::regex::optimize | std::regex::icase);
        }

        inline void setWholeCheckRegex(const std::string& strippedRegex) {
            auto pattern = "^(" + strippedRegex + R"()(?:\/?)$)";
            wholeCheckRegex = getRegex(pattern);
        }

        inline void setPartialCheckRegex(const std::string& strippedRegex) {
            std::string pattern;

            if (strippedRegex[strippedRegex.length() - 1] != '/')
                pattern = "^(" + strippedRegex + R"()(\/[^/\?#]*)*$)";
            else
                pattern = "^(" + strippedRegex + R"()([^/\?#]*\/?)*$)";

            partialCheckRegex = getRegex(pattern);
        }

        std::regex wholeCheckRegex, partialCheckRegex;
        std::string regexString;
        std::vector<std::string> parameterNames;
        bool canHandlePartial;
    };

}

#endif //NEXPRESS_PATHREGEXP_H
#include "pathRegexp.h"

namespace nex {

    PathRegExp::PathRegExp(std::string regex, std::vector<std::string> paramKeys, bool canHandlePartial)
        : regexString(std::move(regex)),
          parameterNames(std::move(paramKeys)),
          canHandlePartial(canHandlePartial)
    {
        if (canHandlePartial) {
            setPartialCheckRegex(regexString);
        } else {
            setWholeCheckRegex(regexString);
        }
    }

    bool PathRegExp::match(const std::string& path, RouteParamMapping& mapping, std::string& basePath,
           std::string& restPath)
    {
        if (canHandlePartial) {
            return matchPartial(path, mapping, basePath, restPath);
        }

        return matchWhole(path, mapping, basePath);
    }

    bool PathRegExp::matchWhole(const std::string& path, RouteParamMapping& mapping, std::string& basePath) {
        std::smatch matchResult;

        if (std::regex_match(path, matchResult, wholeCheckRegex)) {
            mapping.clear();

            if (matchResult.size() > 1) {
                basePath = matchResult[1].str();
            }

            for (size_t i = 2, p = 0; i < matchResult.size() && p < parameterNames.size(); ++i, ++p) {
                mapping[parameterNames[p]] = matchResult[i].str();
            }

            return true;
        }

        return false;
    }

    bool PathRegExp::matchPartial(const std::string& path, RouteParamMapping& mapping, std::string& basePath,
          std::string& restPath)
    {
        std::smatch matchResult;

        if (std::regex_match(path, matchResult, partialCheckRegex)) {
            mapping.clear();

            if (matchResult.size() > 1) {
                basePath = matchResult[1].str();
            }

            for (size_t i = 2, p = 0; i < matchResult.size() && p < parameterNames.size(); ++i, ++p) {
                mapping[parameterNames[p]] = matchResult[i].str();
            }

            restPath = matchResult[matchResult.size() - 1].str();

            return true;
        }

        return false;
    }


} // nex

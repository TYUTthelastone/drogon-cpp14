#include <drogon/plugins/SlashRemover.h>
#include <drogon/plugins/Redirector.h>
#include <drogon/HttpAppFramework.h>
#include "drogon/utils/FunctionTraits.h"
#include <functional>
#include <string>
#include <regex>

using namespace drogon;
using namespace drogon::plugin;
using std::string;

#define TRAILING_SLASH_REGEX ".+\\/$"
#define DUPLICATE_SLASH_REGEX ".*\\/{2,}.*"

enum removeSlashMode : uint8_t
{
    trailing = 1 << 0,
    duplicate = 1 << 1,
    both = trailing | duplicate,
};

inline constexpr const char* regexes[] = {
    TRAILING_SLASH_REGEX,
    DUPLICATE_SLASH_REGEX,
    TRAILING_SLASH_REGEX "|" DUPLICATE_SLASH_REGEX,
};

static inline bool removeTrailingSlashes(string& url)
{
    const auto notSlashIndex = url.find_last_not_of('/');
    if (notSlashIndex == string::npos)  // Root
    {
        url.resize(1);
        return true;
    }
    url.resize(notSlashIndex + 1);
    return false;
}

static inline void removeDuplicateSlashes(string& url)
{
    size_t a = 1, len = url.size();
    for (; a < len && (url[a - 1] != '/' || url[a] != '/'); ++a)
        ;
    for (size_t b = a--; b < len; ++b)
    {
        const char c = url[b];
        if (c != '/' || url[a] != '/')
        {
            ++a;
            url[a] = c;
        }
    }
    url.resize(a + 1);
}

static inline void removeExcessiveSlashes(string& url)
{
    if (url.back() == '/' &&  // This check is so we don't search if there is no
                              // trailing slash to begin with
        removeTrailingSlashes(
            url))  // If it is root path, we don't need to check for duplicates
        return;

    removeDuplicateSlashes(url);
}

void SlashRemover::initAndStart(const Json::Value& config)
{
    trailingSlashes_ = config.get("remove_trailing_slashes", true).asBool();
    duplicateSlashes_ = config.get("remove_duplicate_slashes", true).asBool();
    redirect_ = config.get("redirect", true).asBool();
    const uint8_t removeMode =
        (trailingSlashes_ * trailing) | (duplicateSlashes_ * duplicate);
    if (!removeMode)
        return;
    auto redirector = app().getPlugin<Redirector>();
    redirector->registerHandler(
        [removeMode, redirect = redirect_](const HttpRequestPtr& req,
                                           std::string& location) {
            static const std::regex regex(regexes[removeMode - 1]);
            if (std::regex_match(req->path(), regex))
            {
                string newPath = req->path();
                switch (removeMode)
                {
                    case trailing:
                        removeTrailingSlashes(newPath);
                        break;
                    case duplicate:
                        removeDuplicateSlashes(newPath);
                        break;
                    case both:
                    default:
                        removeExcessiveSlashes(newPath);
                        break;
                }
                if (redirect)
                {
                    req->setPath(newPath);
                    if (location.empty())
                    {
                        location = std::move(newPath);
                    }
                    else
                    {
                        if (location.starts_with("http"))
                        {
                            auto pos = location.find_first_of('/', 8);
                            location.resize(pos);
                        }
                        location.append(newPath);
                        if (!req->query().empty())
                        {
                            location.append("?").append(req->query());
                        }
                    }
                }
                else
                {
                    req->setPath(newPath);
                }
            }
        });
}

void SlashRemover::shutdown()
{
    LOG_TRACE << "SlashRemover plugin is shutdown!";
}

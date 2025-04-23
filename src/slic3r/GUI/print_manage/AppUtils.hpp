#ifndef RemotePrint_DmUtils_hpp_
#define RemotePrint_DmUtils_hpp_
#include "nlohmann/json.hpp"
#include <string>

class wxWebView;
namespace DM{

class AppUtils{
public:
    static void PostMsg(wxWebView*browse, const std::string&data);
    static void PostMsg(wxWebView*browse, nlohmann::json&data);
    static std::string MD5(const std::string&file);

    static std::string extractDomain(const std::string& url);
};
}
#endif
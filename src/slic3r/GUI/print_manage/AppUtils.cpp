#include "AppUtils.hpp"
#include "../Widgets/WebView.hpp"
#include "../GUI.hpp"

#include <boost/uuid/detail/md5.hpp>
#include "libslic3r/Utils.hpp"

using namespace Slic3r::GUI;
using namespace boost::uuids::detail;

namespace DM{

    void AppUtils::PostMsg(wxWebView* browse, const std::string& data)
    {
        WebView::RunScript(browse, from_u8(data));
    }

    void AppUtils::PostMsg(wxWebView* browse, nlohmann::json& data)
    {
        WebView::RunScript(browse, from_u8(wxString::Format("window.handleStudioCmd('%s');", data.dump(-1, ' ', true)).ToStdString()));
    }

    std::string AppUtils::MD5(const std::string& file)
    {
        std::string ret;
        std::string filePath = std::string(file);
        Slic3r::bbl_calc_md5(filePath, ret);
        return ret;
    }

    std::string AppUtils::extractDomain(const std::string& url)
    {
        std::string domain;
        size_t start = 0;

        // 检查是否有协议头（如 http:// 或 https://）
        if (url.find("://") != std::string::npos) {
            start = url.find("://") + 3;
        }

        // 找到域名结束的位置，即第一个 /、? 或 # 字符的位置
        size_t end = url.find_first_of("/?#:", start);
        if (end == std::string::npos) {
            // 如果没有找到结束字符，说明域名一直到字符串末尾
            domain = url.substr(start);
        }
        else {
            // 提取域名部分
            domain = url.substr(start, end - start);
        }

        return domain;
    }
}
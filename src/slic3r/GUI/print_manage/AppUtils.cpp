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

        // ����Ƿ���Э��ͷ���� http:// �� https://��
        if (url.find("://") != std::string::npos) {
            start = url.find("://") + 3;
        }

        // �ҵ�����������λ�ã�����һ�� /��? �� # �ַ���λ��
        size_t end = url.find_first_of("/?#:", start);
        if (end == std::string::npos) {
            // ���û���ҵ������ַ���˵������һֱ���ַ���ĩβ
            domain = url.substr(start);
        }
        else {
            // ��ȡ��������
            domain = url.substr(start, end - start);
        }

        return domain;
    }
}
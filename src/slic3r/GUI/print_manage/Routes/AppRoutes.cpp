#include "AppRoutes.hpp"
#include "nlohmann/json.hpp"
#include "../TypeDefine.hpp"
#include "../AppMgr.hpp"
#include "../AppUtils.hpp"

#include "SysRoutes.hpp"
#include "SenderRoutes.hpp"
#include "DeviceMgrRoutes.hpp"

namespace DM{
    bool AppRoutes::Invoke(wxWebView* browser, const std::string& data)
    {
        nlohmann::json j = nlohmann::json::parse(data);
        if (j.contains("command")) {
            std::string command = j["command"].get<std::string>();

            bool bSysEvent = false;

            Apps apps;
            AppMgr::Ins().GetNeedToReceiveSysEventApp(command, apps);

            for (auto& app : apps) {
                if (app.browser != browser) {
                    bSysEvent = true;
                    AppUtils::PostMsg(app.browser, j);
                }
            }

            if (1) {
                if(SysRoutes().Invoke(browser, j, data))return true;
                if(SenderRoutes().Invoke(browser, j, data))return true;
                if(DeviceMgrRoutes().Invoke(browser, j, data))return true;
            }
        }

        return false;
    }

    AppRoutes::AppRoutes()
    {
       
    }
}
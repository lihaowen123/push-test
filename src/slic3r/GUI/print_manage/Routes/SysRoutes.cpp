#include "SysRoutes.hpp"
#include "nlohmann/json.hpp"
#include "../AppUtils.hpp"
#include "slic3r/GUI/GUI_App.hpp"

using namespace Slic3r::GUI;

namespace DM{
    SysRoutes::SysRoutes()
    {
        //
        this->Handler({"is_dark_theme"}, [](wxWebView*browse, const std::string& data, const nlohmann::json& json_data, const std::string cmd) {
            wxString lan = wxGetApp().current_language_code_safe();
            nlohmann::json commandJson;
            commandJson["command"] = "is_dark_theme";
            commandJson["data"] = wxGetApp().dark_mode();

            AppUtils::PostMsg(browse, commandJson);
            return true;
        });

        //
        this->Handler({"get_user"}, [](wxWebView* browse, const std::string& data, nlohmann::json& json_data, const std::string cmd) {
            auto user = wxGetApp().get_user();
            std::string country_code = wxGetApp().app_config->get("region");
            unsigned pid = Slic3r::get_current_pid();

            nlohmann::json top_level_json = {
                {"bLogin", user.bLogin ? 1 : 0},
                {"token", user.token},
                {"userId", user.userId},
                {"region", country_code},
                {"pid", pid}
            };

            nlohmann::json commandJson = {
                {"command", "get_user"},
                {"data", top_level_json}
            };

            AppUtils::PostMsg(browse, commandJson);
            return true;
            });

        //
        this->Handler({ "common_openurl" }, [](wxWebView* browse, const std::string& data, nlohmann::json& json_data, const std::string cmd) {
            wxLaunchDefaultBrowser(json_data["url"]);

            return true;
            });

        this->Handler({ "get_lang" }, [](wxWebView* browse, const std::string& data, nlohmann::json& json_data, const std::string cmd) {
            wxString lan = wxGetApp().app_config->get("language");
            nlohmann::json commandJson;
            commandJson["command"] = "get_lang";
            commandJson["data"] = lan.ToStdString();

            AppUtils::PostMsg(browse, commandJson);
            return true;
            });

    }

}
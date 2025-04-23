#include "DataCenter.hpp"
namespace DM{

struct DataCenter::priv
{
    nlohmann::json m_data;

    nlohmann::json m_current_device;
    DM::Device m_current_device_data;

    bool m_current_device_changed_state=false;
};

DataCenter::DataCenter():p(new priv)
{
}

void DataCenter::update_data(const nlohmann::json& datas)
{
    p->m_data = datas;

    DM::Device current_device_last = DM::Device::deserialize(p->m_current_device);
    p->m_current_device = this->_get_acive_device();

    p->m_current_device_data = DM::Device::deserialize(p->m_current_device);

    p->m_current_device_changed_state = p->m_current_device_data.address != current_device_last.address;
}

bool DataCenter::is_current_device_changed()
{
    return p->m_current_device_changed_state;
}

const nlohmann::json DataCenter::GetData()
{
    return p->m_data;
}

nlohmann::json DataCenter::find_printer_by_mac(const std::string& device_mac)
{
    try {

        nlohmann::json printerData;
        if(p->m_data.contains("data") && p->m_data["data"].contains("printerList"))
        {
            printerData = p->m_data["data"];
        }
        else
        {
            return nullptr;
        }

        if (!printerData.contains("printerList")) {
            return nullptr;
        }

        nlohmann::json foundPrinter = nullptr;

        for (const auto& group : printerData["printerList"]) 
        {
            if (!group.contains("list"))
            {
                continue;
            }

            for (const auto& printer : group["list"]) 
            {
                if(!printer.contains("mac") || !printer.contains("deviceType"))
                {
                    continue;
                }

                if (printer["mac"] == device_mac) {
                    if (printer["deviceType"] == 0 && printer["online"].get<bool>()) {
                        return printer; // 优先返回 deviceType 为 0 的打印机
                    } else if (printer["deviceType"] == 1) {
                        foundPrinter = printer; // 记录 deviceType 为 1 的打印机
                    }
                }
            }
        }

        return foundPrinter; // 如果没有找到 deviceType 为 0 的打印机，返回 deviceType 为 1 的打印机
    } 
    catch (std::exception& e) 
    {
        return nullptr;
    }

}

nlohmann::json DataCenter::get_current_device()
{
    return p->m_current_device;
}

const DM::Device&DataCenter::get_current_device_data()
{
    return p->m_current_device_data;
}

DM::Device DataCenter::get_printer_data(std::string address)
{
    DM::Device data;
    try {

        nlohmann::json printerData;
        if (p->m_data.contains("data") && p->m_data["data"].contains("printerList"))
        {
            printerData = p->m_data["data"];
        }
        else
        {
            return data;
        }

        if (!printerData.contains("printerList")) {
            return data;
        }

        for (const auto& group : printerData["printerList"])
        {
            if (!group.contains("list"))
            {
                continue;
            }

            for (const auto& printer : group["list"])
            {
                if (!printer.contains("address"))
                {
                    continue;
                }

                if (printer["address"] == address) {
                    nlohmann::json printer_tmp = nlohmann::json(printer);

                     return DM::Device::deserialize(printer_tmp);
                }
            }
        }
    }
    catch (std::exception& e)
    {
        return data;
    }

    return data;
}

bool DataCenter::DeviceHasBoxColor(std::string address)
{
    DM::Device data = get_printer_data(address);
    if(data.valid){
        return data.boxColorInfos.size()>0;
    }

    return false;
}

nlohmann::json DataCenter::_get_acive_device()
{
    nlohmann::json device_local(nullptr), device_cxy(nullptr);
    try
    {
        if (p->m_data.contains("data") && p->m_data["data"].contains("currentActivePrinterMac") && p->m_data["data"].contains("printerList"))
        {
            std::string mac = p->m_data["data"]["currentActivePrinterMac"].get<std::string>();
            for (const auto& group : p->m_data["data"]["printerList"])
            {
                if (!group.contains("list"))
                {
                    continue;
                }

                for (const auto& printer : group["list"])
                {
                    if (!printer.contains("mac") || !printer.contains("deviceType"))
                    {
                        continue;
                    }

                    if (printer["mac"] == mac) {
                        if (printer["deviceType"] == 0 && printer["online"].get<bool>()) {
                            device_local = printer;
                            break;
                        }
                        else if (printer["deviceType"] == 1) {
                            device_cxy = printer;
                        }
                    }
                }

                if (!device_local.empty())break;
            }
        }
    }
    catch (std::exception& e)
    {
        return nullptr;
    }

    if (device_local.empty()) {
        return device_cxy;
    }

    return device_local;
}

DataCenter& DataCenter::Ins()
{
    static DataCenter obj;
    return obj;
}
}
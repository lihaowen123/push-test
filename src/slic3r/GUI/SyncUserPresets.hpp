#ifndef slic3r_SyncUserPresets_hpp_
#define slic3r_SyncUserPresets_hpp_

#include <thread>
#include <mutex>
#include <list>
#include <condition_variable>

#include "CommunicateWithCXCloud.hpp"

namespace Slic3r {
namespace GUI {

class SyncUserPresets
{
public:
    enum class ENSyncCmd {ENSC_NULL, 
        ENSC_SYNC_TO_LOCAL, 
        ENSC_SYNC_TO_CXCLOUD_CREATE, 
        ENSC_SYNC_TO_FRONT_PAGE,         //  同步数据到前端页面
        ENSC_SYNC_CONFIG_TO_CXCLOUD    //  同步配置到创想云
    };

    enum class ENSyncThreadState {
        ENTS_IDEL_CHECK,        //  空闲状态，检查是否需要同步
        ENTS_SYNC_TO_LOCAL,     //  同步到本地
        ENTS_SYNC_TO_FRONT_PAGE //  同步到前端
    };

    static SyncUserPresets& getInstance();

    int startup();      //  线程的启动
    void shutdown();    //  线程的结束
    void startSync();   //  同步工作的启动
    void stopSync();    //  同步工作的不启动
    void syncUserPresetsToLocal();
    void syncUserPresetsToCXCloud();
    void syncUserPresetsToFrontPage();
    void syncConfigToCXCloud();

private:
    SyncUserPresets();
    ~SyncUserPresets();

protected:
    void onRun();
    void reloadPresetsInUiThread();
    int  doSyncToLocal(SyncToLocalRetInfo& syncToLocalRetInfo);
    int  doCheckNeedSyncToCXCloud();
    int  doCheckNeedSyncPrinterToCXCloud();
    int  doCheckNeedSyncFilamentToCXCloud();
    int  doCheckNeedSyncProcessToCXCloud();
    int  doCheckNeedDeleteFromCXCloud();
    //  同步配置文件Creality.conf
    int  doCheckNeedSyncConfigToCXCloud();
    int  getSyncDataToFile(std::string& outJsonFile);
    int  copyOldPresetToBackup();
    int  getLocalUserPresets(std::vector<LocalUserPreset>& vtLocalUserPreset);
    int  delLocalUserPresetsInUiThread(const SyncToLocalRetInfo& syncToLocalRetInfo);

protected:
    std::thread m_thread;
    std::atomic_bool     m_bRunning = false;
    std::atomic_bool     m_bSync    = false;
    std::atomic_bool         m_bStoped  = false;
    std::mutex               m_mutexQuit;
    std::condition_variable  m_cvQuit;
    std::list<ENSyncCmd> m_lstSyncCmd;
    std::mutex           m_mutexLstSyncCmd;
    CommunicateWithCXCloud m_commWithCXCloud;
    CommunicateWithFrontPage m_commWithFrontPage;

    ENSyncThreadState m_syncThreadState = ENSyncThreadState::ENTS_IDEL_CHECK;
};

}
}

#endif

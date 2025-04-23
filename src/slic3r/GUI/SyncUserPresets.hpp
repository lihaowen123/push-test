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
        ENSC_SYNC_TO_FRONT_PAGE,         //  ͬ�����ݵ�ǰ��ҳ��
        ENSC_SYNC_CONFIG_TO_CXCLOUD    //  ͬ�����õ�������
    };

    enum class ENSyncThreadState {
        ENTS_IDEL_CHECK,        //  ����״̬������Ƿ���Ҫͬ��
        ENTS_SYNC_TO_LOCAL,     //  ͬ��������
        ENTS_SYNC_TO_FRONT_PAGE //  ͬ����ǰ��
    };

    static SyncUserPresets& getInstance();

    int startup();      //  �̵߳�����
    void shutdown();    //  �̵߳Ľ���
    void startSync();   //  ͬ������������
    void stopSync();    //  ͬ�������Ĳ�����
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
    //  ͬ�������ļ�Creality.conf
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

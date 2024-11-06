// ExternalClient.h
#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <string>

class ExternalClient {
public:
    ExternalClient(const std::string& uid);

    void setPpDevice(const std::string& ppdev);
    std::string getPpDevice() const;

    void setPpSampleInterval(const std::string& sampInt);
    std::string getPpSampleInterval() const;

    void setUploadIntervalMin(int uploadInt);
    int getUploadIntervalMin() const;

    void scheduleDeletion();
    std::map<std::string, std::string> getMsmtSettings() const;

    int scheduleRequest(Job job);
    Job getNextRequest();
    void setResponse(const Response& response);

    bool responseArrived(int requestNo) const;
    Response getResponse(int requestNo);
    bool waitForResponseTo(int requestNo);
    void purgeRequestNo(int requestNo);

private:
    std::string uid;
    std::queue<Job> jobqueue;
    std::map<int, Response> responsedict;
    mutable std::mutex responsedictlock;
    std::condition_variable responsedictcv;
    std::mutex seqnoLock;
    int lastRequestNo;

    // Standard measurement settings
    std::string ppDevice;
    std::string ppSamplingInterval;
    int uploadIntervalMin;
};

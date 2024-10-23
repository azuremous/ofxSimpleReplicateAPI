/*-----------------------------------------------------------/
ofxSimpleReplicateAPI.h

github.com/azuremous
Created by Jung un Kim a.k.a azuremous on 10/03/24.
/----------------------------------------------------------*/

#pragma once

#include "ofThread.h"
#include "ofxSimpleRestAPI.h"

typedef enum {
    UPLOAD_IMAGE_REQUEST,
    API_REQUEST,
    WAIT_REQUEST,
    DOWNLOAD_REQUEST
} REPLICATE_REQUEST_TYPE;

class ofxSimpleReplicateAPI : ofThread {
private:
    REPLICATE_REQUEST_TYPE currentRequestType;
    ofThreadChannel<string>request;
    ofxSimpleRestAPI restAPI;
    string key;

    int waitSec;
    int waitTime;

    bool useDetailLog;
    bool resultIsArray;

protected:

    string base64Encode(const string& in) {
        static const string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        string out;
        int val = 0, valb = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4) out.push_back('=');
        return out;
    }

    string extractUrl(const std::string& jsonString) {
        try {
            ofJson jsonData = ofJson::parse(jsonString);
            return jsonData["urls"]["get"];
        }
        catch (ofJson::exception& e) {
            ofLogError() << "Error parsing JSON: " << e.what();
            return "";
        }
    };

    void threadedFunction() {
        string url;
        while (isThreadRunning()) {
            if (request.receive(url)) {
                int status = restAPI.getResponseStatus();
                if (currentRequestType != WAIT_REQUEST) {
                    if (status == 201) {
                        string result = extractUrl(restAPI.getData());
                        ofNotifyEvent(resultURL, result);
                    }
                    else {
                        if (status == 503) {
                            string result = "503";
                            ofNotifyEvent(resultURL, result);
                        }
                    }
                }
                else if (currentRequestType == WAIT_REQUEST) {
                    int waitCounter = waitSec;
                    ofSleepMillis(waitCounter * 1000);

                    int waitMills = waitTime * 1000;

                    while (status != 200 || restAPI.getData().find("\"status\":\"succeeded\"") == string::npos) {
                        if (status == 200 && restAPI.getData().find("\"status\":\"failed\"") != string::npos) {
                            string result = "failed";
                            ofNotifyEvent(resultURL, result);
                            break;
                        }
                        else {
                            if (currentRequestType == WAIT_REQUEST) {
                                // Wait and keep polling
                                ofSleepMillis(waitMills);
                                restAPI.setRequest(url, ofHttpRequest::GET, 10);
                                restAPI.addHeader("Authorization", "Bearer " + key);
                                status = restAPI.getResponseStatus();
                                waitCounter += waitTime;
                            }
                            else {
                                break;
                            }
                        }
                    }

                    if (status == 200 && currentRequestType == WAIT_REQUEST) {
                        cout << "result ------------:" << restAPI.getData() << endl;
                        if (resultIsArray) {
                            vector<string> outputUrls = restAPI.getData<vector<string>>("output");
                            if (outputUrls.size() > 0) {
                                string result = outputUrls[0];  // Extract the first image URL from the output array
                                ofNotifyEvent(resultURL, result);
                            }
                        }
                        else {
                            string outputURL = restAPI.getData<string>("output");
                            if (outputURL != "") {
                                ofNotifyEvent(resultURL, outputURL);
                            }
                        }
                    }
                }
            }
        }

    }

public:
    ofEvent<string>resultURL;

    ofxSimpleReplicateAPI() :currentRequestType(DOWNLOAD_REQUEST), key(""), waitSec(0), waitTime(0), useDetailLog(false), resultIsArray(false)
    {

    }

    ~ofxSimpleReplicateAPI() {
        request.close();
        stopThread();
    }

    void setup(const string& key, bool useDetailLog = false) {
        this->key = key;
        this->useDetailLog = useDetailLog;
        if (useDetailLog) {
            restAPI.showDetailLog();
        }
        currentRequestType = DOWNLOAD_REQUEST;
        startThread();
    }

    void setCAPath(const string& s) {
        restAPI.setCAPath(s);
    }

    void reset() {
        currentRequestType = DOWNLOAD_REQUEST;
        restAPI.clear();
        resultIsArray = false;
        waitSec = 0;
        waitTime = 0;
    }

    void uploadImage(const string& path, const string& type = "png") {
        currentRequestType = UPLOAD_IMAGE_REQUEST;
        ofBuffer imgBuf = ofBufferFromFile(path);
        string name = "u" + ofGetTimestampString() + "." + type;
       
        restAPI.setRequestData(imgBuf, "content", name);
        restAPI.setRequest("https://api.replicate.com/v1/files", ofHttpRequest::Method::POST, 0, "multipart/form-data");
        restAPI.addHeader("Authorization", "Bearer " + key);

        request.send("");
    }

    string uploadImage(const string &url, const string & username, const string & password, const string& path, const string& type = "png") {
        currentRequestType = UPLOAD_IMAGE_REQUEST;
        ofBuffer imgBuf = ofBufferFromFile(path);
        string name = "u" + ofGetTimestampString() + "." + type;
        restAPI.setRequestData(imgBuf, "photo", name);
        string auth = "Basic "+base64Encode(username + ":" + password);
        restAPI.setRequest(url, ofHttpRequest::Method::POST, 0, "multipart/form-data", auth);
        int result = restAPI.getResponseStatus();
        if (result == 200) {
            return name;
        }
        return "";
    }

    void setRequest(const string& body, const bool& resultIsArray = false, const string& address = "https://api.replicate.com/v1/predictions") {
        currentRequestType = API_REQUEST;
        this->resultIsArray = resultIsArray;
        restAPI.clear();
        restAPI.setRequest(address, ofHttpRequest::Method::POST, 0, "application/json");
        restAPI.addHeader("Authorization", "Bearer " + key);
        restAPI.setRequestBody(body);
        request.send("");
    }

    void waitProcess(const string& url, int waitSec = 35, int waitTime = 1) {
        currentRequestType = WAIT_REQUEST;
        this->waitSec = waitSec;
        this->waitTime = waitTime;
        restAPI.setRequest(url, ofHttpRequest::GET, 10);
        restAPI.addHeader("Authorization", "Bearer " + key);
        request.send(url);
    }

    void downloadImage(const string& url, const string& name) {
        currentRequestType = DOWNLOAD_REQUEST;
        restAPI.setRequest(url, ofHttpRequest::GET, 10);
        restAPI.saveToFile(name);
        int imageGetAPIStaus = restAPI.getResponseStatus();
        string result = ofToString(imageGetAPIStaus);
        ofNotifyEvent(resultURL, result);
    }
};

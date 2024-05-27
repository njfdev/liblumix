#pragma once

#include <iostream>
#include <vector>
#include <variant>
#include <fmt/core.h>
#include <optional>
#include <string>
#include <algorithm>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <pugixml.hpp>

// only include pugixml if the environment is build
#if defined LIBLUMIX_BUILD || defined __INTELLISENSE__
#include <cpr/cpr.h>
#include "build/_deps/cppcodec-src/cppcodec/hex_lower.hpp"
#include <jpeglib.h>
#endif

namespace Lumix {
    struct CameraData {
        std::string cameraIp;
        int apiVersion[2]; // [major, minor]
        std::string cameraName;
        std::string manufacturer;
        std::string modelName;
        std::string modelNumber;
        std::string serialNumber;
        std::string udn; // Universal Device Name
        int firmwareVersion[2]; // [major, minor]
    };

    struct ImageData {
        uint32_t id;
        std::string title;
        std::string date;
        std::string filename;
        std::vector<unsigned char> rawFileData;
        std::vector<unsigned char> pixelBuffer;
    };

    class Camera {
    public:
        Camera(std::string cameraIp, std::string nameForConnection);
        ~Camera();

        CameraData cameraData;

        bool TakePhoto();
        bool TakePhoto(float duration);
        bool DownloadLatestPhoto(ImageData& imageData);
        bool GetRawPixelData(ImageData& imageData);

    private:
        enum CameraRequestMode {
            SetSetting,
            GetInfo,
            GetSetting,
            GetState,
            GetContentInfo,
            CameraCommand,
            CameraControl,
            StartStream
        };

        enum GetSettingRequestType {
            PA,
            // Ex_Tele_Conv,
            TouchType
        };

        enum SetSettingRequestType {
            DisplayName,
            Focal, // Aperture
            ISO,
            Exposure,
            ShutterSpeed,
            Peaking,
            Filter,
            ColorMode,
            FocusMode,
            LightMettering,
            VideoQuality
        };

        enum GetInfoRequestType {
            Capability,
            AllMenu,
            CurrentMenu,
            Lens
        };

        enum CameraCommandRequestType {
            PlayMode,
            AutoReviewUnlock,
            MenuEntry,
            VideoRecordStart,
            VideoRecordStop,
        };

        enum CameraControlRequestType {
            TouchTrace,
            AssistDisplay,
            ChangeDisplayMagnification,
            Focus
        };

        using CameraRequestType = std::variant<GetSettingRequestType, SetSettingRequestType, GetInfoRequestType, CameraCommandRequestType, CameraControlRequestType>;

        std::string sessionId;

        // separate thread variables
        std::unique_ptr<std::thread> getStateThread;
        std::atomic<bool> getStateThreadRunning = false;
        std::condition_variable getStateThreadCondition;
        std::mutex getStateThreadMutex;

        bool Connect(std::string cameraIp, std::string nameForConnection);
        pugi::xml_node SendCameraCommand(CameraRequestMode mode, std::optional<CameraRequestType> type, std::vector<std::string> params);

        bool GetPixelDataFromJPG(std::vector<unsigned char>& jpgFileData, std::vector<unsigned char>& pixelBuffer);

        // thread functions
        void GetStateThread();
    };
}
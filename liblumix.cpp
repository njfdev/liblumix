#include "liblumix.h"

// add Lumix namespace so we don't have to write Lumix:: everywhere
using namespace Lumix;
using namespace pugi;
using namespace cppcodec;

Camera::Camera(std::string cameraIp, std::string nameForConnection) {
    cameraData.cameraIp = cameraIp;

    if (!Connect(cameraIp, nameForConnection)) {
        throw std::runtime_error("Failed to connect to camera");
    }

    // start GetState thread
    getStateThreadRunning = true;
    getStateThread = std::make_unique<std::thread>(&Camera::GetStateThread, this);
}

Camera::~Camera() {
    // stop the GetState thread
    getStateThreadRunning = false;
    getStateThreadCondition.notify_all();
    if (getStateThread->joinable()) {
        getStateThread->join();
    }
}

xml_node Camera::SendCameraCommand(CameraRequestMode mode, std::optional<CameraRequestType> type, std::vector<std::string> params) {
    std::string modeStr, typeStr;

    switch (mode) {
    case CameraCommand:
        modeStr = "camcmd";
        break;
    case CameraControl:
        modeStr = "camctrl";
        break;
    case StartStream:
        modeStr = "startstream";
        break;
    case GetSetting:
        modeStr = "getsetting";
        break;
    case SetSetting:
        modeStr = "setsetting";
        break;
    case GetState:
        modeStr = "getstate";
        break;
    case GetInfo:
        modeStr = "getinfo";
        break;
    case GetContentInfo:
        modeStr = "get_content_info";
        break;
    }

    if (type.has_value()) {
        // switch based on the variant type
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, GetSettingRequestType>) {
                switch (arg) {
                case PA:
                    typeStr = "pa";
                    break;
                case TouchType:
                    typeStr = "touch_type";
                    break;
                }
            } else if constexpr (std::is_same_v<T, SetSettingRequestType>) {
                switch (arg) {
                case DisplayName:
                    typeStr = "device_name";
                    break;
                case Focal:
                    typeStr = "focal";
                    break;
                case ISO:
                    typeStr = "iso";
                    break;
                case Exposure:
                    typeStr = "exposure";
                    break;
                case ShutterSpeed:
                    typeStr = "shtrspeed";
                    break;
                case Peaking:
                    typeStr = "peaking";
                    break;
                case Filter:
                    typeStr = "filter_setting";
                    break;
                case ColorMode:
                    typeStr = "colormode";
                    break;
                case FocusMode:
                    typeStr = "focusmode";
                    break;
                case LightMettering:
                    typeStr = "lightmetering";
                    break;
                case VideoQuality:
                    typeStr = "videoquality";
                    break;
                }
            } else if constexpr (std::is_same_v<T, GetInfoRequestType>) {
                switch (arg) {
                case Capability:
                    typeStr = "capability";
                    break;
                case AllMenu:
                    typeStr = "allmenu";
                    break;
                case CurrentMenu:
                    typeStr = "curmenu";
                    break;
                case Lens:
                    typeStr = "lens";
                    break;
                }
            } else if constexpr (std::is_same_v<T, CameraCommandRequestType>) {
                switch (arg) {
                case PlayMode:
                    typeStr = "playmode";
                    break;
                case AutoReviewUnlock:
                    typeStr = "autoreviewunlock";
                    break;
                case MenuEntry:
                    typeStr = "menu_entry";
                    break;
                case VideoRecordStart:
                    typeStr = "video_recstart";
                    break;
                case VideoRecordStop:
                    typeStr = "video_recstop";
                    break;
                }
            } else {
                throw std::runtime_error("Invalid CameraRequestType");
            }
        }, type.value());
    }

    cpr::Parameters parameters = cpr::Parameters{{"mode", modeStr}};
    if (typeStr.length() > 0) {
        parameters.Add({"type", typeStr});
    }
    // add the parameters for param "value#" where # is the index and the first param is just "value"
    for (int i = 0; i < params.size(); i++) {
        std::string paramName = "value";
        if (i > 0) {
            paramName += std::to_string(i+1);
        }
        parameters.Add({paramName, params[i]});
    }

    cpr::Header headers = cpr::Header{};
    if (sessionId.length() > 0) {
        headers.insert({"X-SESSION_ID", sessionId});
    }

    cpr::Response r = cpr::Get(cpr::Url{"http://" + cameraData.cameraIp + ":80/cam.cgi"}, parameters, headers);

    xml_document doc;
    xml_parse_result result = doc.load_string(r.text.c_str());

    if (!result) {
        throw std::runtime_error("Failed to parse XML response");
    }

    xml_node camrply = doc.child("camrply");

    if (camrply.value() == "") {
        throw std::runtime_error("Failed to get camrply node");
    }

    return camrply;
}

bool Camera::Connect(std::string cameraIp, std::string nameForConnection) {
    // quickly check if port 60606 is open (to avoid long wait time if camera is off)
    cpr::Response r = cpr::Get(cpr::Url{"http://" + cameraIp + ":60606"}, cpr::Timeout{5000});

    if (r.status_code != 404) {
        return false;
    }

    // get camera info
    r = cpr::Get(cpr::Url{"http://" + cameraIp + ":60606/Lumix/Server0/ddd"});

    if (r.status_code != 200) {
        return false;
    }

    // parse XML
    xml_document doc;
    xml_parse_result result = doc.load_string(r.text.c_str());

    if (!result) {
        return false;
    }

    xml_node root = doc.child("root");
    xml_node device = root.child("device");
    xml_node specVersion = device.child("specVersion");

    cameraData.apiVersion[0] = specVersion.child("major").text().as_int();
    cameraData.apiVersion[1] = specVersion.child("minor").text().as_int();

    cameraData.cameraName = device.child("friendlyName").text().as_string();
    cameraData.manufacturer = device.child("manufacturer").text().as_string();
    cameraData.modelName = device.child("modelName").text().as_string();
    cameraData.modelNumber = device.child("modelNumber").text().as_string();
    cameraData.serialNumber = device.child("serialNumber").text().as_string();
    std::string udn = device.child("UDN").text().as_string();
    // remove the "uuid:" prefix
    cameraData.udn = udn.substr(5);
    std::string firmwareVersion = device.child("pana:X_FirmVersion").text().as_string();
    cameraData.firmwareVersion[0] = std::stoi(firmwareVersion.substr(0, firmwareVersion.find('.')));
    cameraData.firmwareVersion[1] = std::stoi(firmwareVersion.substr(firmwareVersion.find('.') + 1));

    // send a request to start connection
    r = cpr::Get(cpr::Url{"http://" + cameraIp + ":80/cam.cgi"}, cpr::Parameters{{"mode", "accctrl"}, {"type", "req_acc_g"}});
    if (r.status_code != 200) {
        return false;
    }

    // send a request with connection data until we get an "ok" response
    while (true) {
        // TODO: fully understand how to properly use these parameters
        /*
        These parameters are determined from this reverse engineering project: https://github.com/cleverfox/lumixproto

        From this project, it appears the first parameter is the UDN of the camera, and the second parameter is the name
        of the connection. However, it seems my new camera with the latest firmware has some sort of hexadecimal encoding
        that I couldn't determine. Therefore, when connecting to the camera, it will display random characters for the
        connection name rather than the actual name. I will need to investigate this further.

        For now, I will just guess and use base16 encoding for the parameters. However, it appears that later connections do not need
        the encoding.
        */
        std::string udn = hex_lower::encode(cameraData.udn);
        std::string name = hex_lower::encode(nameForConnection);
        r = cpr::Get(cpr::Url{"http://" + cameraIp + ":80/cam.cgi"}, cpr::Parameters{{"mode", "accctrl"}, {"type", "req_acc_e"}, {"value", udn}, {"value2", name}});

        if (r.status_code != 200) {
            return false;
        }

        std::vector<std::string> response = cpr::util::split(r.text, ',');

        if (response[0] == "ok") {
            // get the session ID (last element)
            sessionId = response[response.size() - 1];
            break;
        }

        // sleep for 0.5 seconds
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // set the connection name with the session ID to confirm connection
    xml_node response = SendCameraCommand(SetSetting, DisplayName, {nameForConnection});

    if (response.child("result").text().as_string() != std::string("ok")) {
        return false;
    }
    
    return true;
}

void Camera::GetStateThread() {
    while (getStateThreadRunning) {
        // get the state every 1 second (but don't do anything with it)
        SendCameraCommand(GetState, {}, {});

        // sleep for 1 second
        std::unique_lock<std::mutex> lock(getStateThreadMutex);
        getStateThreadCondition.wait_for(lock, std::chrono::seconds(1));
    }
}

bool Camera::TakePhoto() {
    SendCameraCommand(CameraCommand, {}, {"recmode"});
    xml_node response = SendCameraCommand(CameraCommand, {}, {"capture"});

    response.print(std::cout);

    if (response.child("result").text().as_string() != std::string("ok")) {
        return false;
    }

    return true;
}

bool Camera::TakePhoto(float duration) {
    SendCameraCommand(CameraCommand, {}, {"recmode"});
    SendCameraCommand(SetSetting, ShutterSpeed, {"16384/256"});
    xml_node response = SendCameraCommand(CameraCommand, {}, {"capture"});

    if (response.child("result").text().as_string() != std::string("ok")) {
        return false;
    }

    // wait for the photo to be taken
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(duration * 1000)));

    // cancel the capture
    xml_node response2 = SendCameraCommand(CameraCommand, {}, {"capture_cancel"});

    if (response2.child("result").text().as_string() != std::string("ok")) {
        return false;
    }

    return true;
}

bool Camera::DownloadLatestPhoto(ImageData& imageData) {
    // switch to playmode
    SendCameraCommand(CameraCommand, {}, {"playmode"});

    // get content info
    xml_node response = SendCameraCommand(GetContentInfo, {}, {});

    // get the latest photo position
    int position = response.child("current_position").text().as_int();

    // make request to download the photo
    std::string xmlString = fmt::format(R"(<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
        <u:Browse xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1" xmlns:pana="urn:schemas-panasonic-com:pana">
            <ObjectID>0</ObjectID>
            <BrowseFlag>BrowseDirectChildren</BrowseFlag>
            <Filter>*</Filter>
            <StartingIndex>{}</StartingIndex>
            <RequestedCount>1</RequestedCount>
            <SortCriteria></SortCriteria>
            <pana:X_FromCP>LumixLink2.0</pana:X_FromCP>
            <pana:X_Filter></pana:X_Filter>
        </u:Browse>
    </s:Body>
</s:Envelope>)", position);

    // get the list of photos
    cpr::Response r = cpr::Post(cpr::Url{"http://" + cameraData.cameraIp + ":60606/Server0/CDS_control"}, cpr::Body{xmlString}, cpr::Header{{"Content-Type", "text/xml; charset=\"utf-8\""}, {"SOAPACTION", "\"urn:schemas-upnp-org:service:ContentDirectory:1#Browse\""}});

    if (r.status_code != 200) {
        return false;
    }

    // parse XML
    xml_document doc;
    xml_parse_result result = doc.load_string(r.text.c_str());

    if (!result) {
        return false;
    }

    xml_node envelope = doc.child("s:Envelope");
    xml_node body = envelope.child("s:Body");
    xml_node browseResponse = body.child("u:BrowseResponse");
    xml_node resultNode = browseResponse.child("Result");

    // get the value of the result node and url decode it
    std::string resultValue = resultNode.text().as_string();
    std::string decodedResult = cpr::util::urlDecode(resultValue);

    // parse the XML
    xml_document doc2;
    xml_parse_result result2 = doc2.load_string(decodedResult.c_str());

    if (!result2) {
        return false;
    }

    xml_node root = doc2.child("DIDL-Lite");
    xml_node item = root.child("item");

    std::vector<std::string> resList;

    // go through all the res nodes and get the URLs
    for (xml_node res : item.children("res")) {
        resList.push_back(res.text().as_string());
    }

    // set the id, title, and date
    imageData.id = item.attribute("id").as_uint();
    imageData.title = item.child("dc:title").text().as_string();
    imageData.date = item.child("dc:date").text().as_string();

    // get the first RW2 image, if it doesn't exist, get the first JPG image
    std::string imageDownloadUrl;
    for (std::string res : resList) {
        if (res.find(".RW2") != std::string::npos) {
            imageDownloadUrl = res;
            break;
        }
    }

    if (imageDownloadUrl.length() == 0) {
        for (std::string res : resList) {
            if (res.find(".JPG") != std::string::npos) {
                imageDownloadUrl = res;
                break;
            }
        }
    }

    // split the url by "/" and get the last element (the file name)
    imageData.filename = imageDownloadUrl.substr(imageDownloadUrl.find_last_of("/") + 1);

    std::cout << "Downloading image: " << imageDownloadUrl << std::endl;
    std::cout << "Filename: " << imageData.filename << std::endl;

    // download the image
    r = cpr::Get(cpr::Url{imageDownloadUrl});

    if (r.status_code != 200) {
        return false;
    }

    imageData.rawFileData = std::vector<unsigned char>(r.text.begin(), r.text.end());

    return true;
}

bool Camera::GetRawPixelData(ImageData& imageData) {
    // if the file is a RW2, get the pixel data
    if (imageData.filename.find(".RW2") != std::string::npos) {
        if (!GetPixelDataFromRW2(imageData)) {
            return false;
        }
        return true;
    } else if (imageData.filename.find(".JPG") != std::string::npos) {
        if (!GetPixelDataFromJPG(imageData)) {
            return false;
        }
        return true;
    }

    std::cout << "Unsupported file type" << std::endl;

    return false;
}

bool Camera::GetPixelDataFromJPG(ImageData& imageData) {
    // read using libjpeg
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, imageData.rawFileData.data(), imageData.rawFileData.size());

    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    imageData.width = cinfo.output_width;
    imageData.height = cinfo.output_height;
    imageData.channels = cinfo.output_components;
    imageData.bit_depth = cinfo.data_precision;

    // size of a row
    int row_stride = imageData.width * imageData.channels * imageData.bit_depth / 8;

    // save the pixel data
    imageData.pixelBuffer.resize(row_stride * imageData.height);

    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    for (int i = 0; i < imageData.height; i++) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        std::memcpy(imageData.pixelBuffer.data() + i * row_stride, buffer[0], row_stride);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    // swap the red and blue channels
    for (int i = 0; i < imageData.pixelBuffer.size(); i += 3) {
        unsigned char temp = imageData.pixelBuffer[i];
        imageData.pixelBuffer[i] = imageData.pixelBuffer[i + 2];
        imageData.pixelBuffer[i + 2] = temp;
    }

    return true;
}

bool Camera::GetPixelDataFromRW2(ImageData& imageData) {
    // read using LibRaw
    libraw_data_t *processor = libraw_init(0);
    if (processor == NULL) {
        return false;
    }

    int ret = libraw_open_buffer(processor, imageData.rawFileData.data(), imageData.rawFileData.size());
    if (ret != LIBRAW_SUCCESS) {
        libraw_close(processor);
        return false;
    }

    ret = libraw_unpack(processor);
    if (ret != LIBRAW_SUCCESS) {
        libraw_close(processor);
        return false;
    }

    imageData.width = processor->sizes.width;
    imageData.height = processor->sizes.height;
    imageData.channels = processor->idata.colors;
    imageData.bit_depth = processor->params.output_bps;

    ret = libraw_dcraw_process(processor);
    if (ret != LIBRAW_SUCCESS) {
        libraw_close(processor);
        return false;
    }

    libraw_processed_image_t *image = libraw_dcraw_make_mem_image(processor, &ret);
    if (image == NULL) {
        libraw_close(processor);
        return false;
    }

    // save the pixel data
    imageData.pixelBuffer.resize(image->data_size);
    std::memcpy(imageData.pixelBuffer.data(), image->data, image->data_size);

    libraw_dcraw_clear_mem(image);

    libraw_close(processor);

    return true;
}
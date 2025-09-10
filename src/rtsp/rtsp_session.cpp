/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_session.h"

#include <rtsp/rtsp_logger.h>

#include <ctime>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>

#include "media_stream.h"
#include "rtsp/rtsp_headers.h"
#include "rtsp/rtsp_session_state.h"
#include "rtsp_response.h"
#include "rtsp_server.h"

namespace lmshao::rtsp {

RTSPSession::RTSPSession(std::shared_ptr<network::Session> networkSession)
    : networkSession_(networkSession), timeout_(60)
{ // Default 60 seconds timeout

    // Generate session ID
    sessionId_ = GenerateSessionId();

    // Initialize last active time
    lastActiveTime_ = std::time(nullptr);

    // Initialize state machine to Initial state
    currentState_ = InitialState::GetInstance();

    RTSP_LOGD("RTSPSession created with ID: %s", sessionId_.c_str());
}

RTSPSession::RTSPSession(std::shared_ptr<network::Session> networkSession, std::weak_ptr<RTSPServer> server)
    : networkSession_(networkSession), rtspServer_(server), timeout_(60)
{
    // Generate session ID
    sessionId_ = GenerateSessionId();

    // Initialize last active time
    lastActiveTime_ = std::time(nullptr);

    // Initialize state machine to Initial state
    currentState_ = InitialState::GetInstance();

    RTSP_LOGD("RTSPSession created with ID: %s and server reference", sessionId_.c_str());
}

RTSPSession::~RTSPSession()
{
    RTSP_LOGD("RTSPSession destroyed: %s", sessionId_.c_str());

    // Clean up media streams
    mediaStreams_.clear();
}

RTSPResponse RTSPSession::ProcessRequest(const RTSPRequest &request)
{
    // Update last active time
    UpdateLastActiveTime();

    // Use state machine to process request
    if (!currentState_) {
        // Fallback: initialize to Initial state if not set
        currentState_ = InitialState::GetInstance();
    }

    const std::string &method = request.method_;

    // Delegate to state machine based on method
    if (method == "OPTIONS") {
        return currentState_->OnOptions(this, request);
    } else if (method == "DESCRIBE") {
        return currentState_->OnDescribe(this, request);
    } else if (method == "ANNOUNCE") {
        return currentState_->OnAnnounce(this, request);
    } else if (method == "RECORD") {
        return currentState_->OnRecord(this, request);
    } else if (method == "SETUP") {
        return currentState_->OnSetup(this, request);
    } else if (method == "PLAY") {
        return currentState_->OnPlay(this, request);
    } else if (method == "PAUSE") {
        return currentState_->OnPause(this, request);
    } else if (method == "TEARDOWN") {
        return currentState_->OnTeardown(this, request);
    } else if (method == "GET_PARAMETER") {
        return currentState_->OnGetParameter(this, request);
    } else if (method == "SET_PARAMETER") {
        return currentState_->OnSetParameter(this, request);
    } else {
        // Unknown method
        int cseq = 0;
        auto cseq_it = request.general_header_.find("CSeq");
        if (cseq_it != request.general_header_.end()) {
            cseq = std::stoi(cseq_it->second);
        }
        return RTSPResponseBuilder().SetStatus(StatusCode::NotImplemented).SetCSeq(cseq).Build();
    }
}

void RTSPSession::ChangeState(std::shared_ptr<RTSPSessionState> newState)
{
    currentState_ = newState;
}

std::shared_ptr<RTSPSessionState> RTSPSession::GetCurrentState() const
{
    return currentState_;
}

std::string RTSPSession::GetSessionId() const
{
    return sessionId_;
}

std::string RTSPSession::GetClientIP() const
{
    if (networkSession_) {
        return networkSession_->host;
    }
    return "";
}

uint16_t RTSPSession::GetClientPort() const
{
    if (networkSession_) {
        return networkSession_->port;
    }
    return 0;
}

std::shared_ptr<network::Session> RTSPSession::GetNetworkSession() const
{
    return networkSession_;
}

std::weak_ptr<RTSPServer> RTSPSession::GetRTSPServer() const
{
    return rtspServer_;
}

bool RTSPSession::SetupMedia(const std::string &uri, const std::string &transport)
{
    RTSP_LOGD("Setting up media for URI: %s, Transport: %s", uri.c_str(), transport.c_str());

    // Parse transport parameters
    rtpTransportParams_ = ParseTransportHeader(transport);

    // Extract client ports from transport header
    uint16_t clientRtpPort = 0, clientRtcpPort = 0;
    size_t clientPortPos = transport.find("client_port=");
    if (clientPortPos != std::string::npos) {
        size_t portStart = clientPortPos + 12; // Length of "client_port="
        size_t portEnd = transport.find(';', portStart);
        if (portEnd == std::string::npos) {
            portEnd = transport.length();
        }
        std::string portRange = transport.substr(portStart, portEnd - portStart);
        size_t dashPos = portRange.find('-');
        if (dashPos != std::string::npos) {
            clientRtpPort = static_cast<uint16_t>(std::stoi(portRange.substr(0, dashPos)));
            clientRtcpPort = static_cast<uint16_t>(std::stoi(portRange.substr(dashPos + 1)));
        }
    }

    // Allocate server ports (simple allocation for demo)
    uint16_t serverRtpPort = 6000 + (std::hash<std::string>{}(sessionId_) % 1000) * 2;
    uint16_t serverRtcpPort = serverRtpPort + 1;

    // Build transport info for response
    transportInfo_ = transport + ";server_port=" + std::to_string(serverRtpPort) + "-" + std::to_string(serverRtcpPort);

    // Set setup flag
    isSetup_ = true;

    RTSP_LOGD("Media setup completed for session: %s, Transport: %s", sessionId_.c_str(), transportInfo_.c_str());
    return true;
}

bool RTSPSession::PlayMedia(const std::string &uri, const std::string &range)
{
    RTSP_LOGD("Playing media for URI: %s, Range: %s", uri.c_str(), range.c_str());

    if (!isSetup_) {
        RTSP_LOGE("Cannot play media: session not setup");
        return false;
    }

    // Set playing state
    isPlaying_ = true;
    isPaused_ = false;

    RTSP_LOGD("Media playback started for session: %s", sessionId_.c_str());
    return true;
}

bool RTSPSession::PauseMedia(const std::string &uri)
{
    RTSP_LOGD("Pausing media for URI: %s", uri.c_str());

    if (!isPlaying_) {
        RTSP_LOGE("Cannot pause media: not currently playing");
        return false;
    }

    // Set paused state
    isPaused_ = true;
    isPlaying_ = false;

    RTSP_LOGD("Media playback paused for session: %s", sessionId_.c_str());
    return true;
}

bool RTSPSession::TeardownMedia(const std::string &uri)
{
    RTSP_LOGD("Tearing down media for URI: %s", uri.c_str());

    // Reset all states
    isPlaying_ = false;
    isPaused_ = false;
    isSetup_ = false;

    // Clear media streams
    mediaStreams_.clear();

    RTSP_LOGD("Media teardown completed for session: %s", sessionId_.c_str());
    return true;
}

void RTSPSession::SetSdpDescription(const std::string &sdp)
{
    sdpDescription_ = sdp;
}

std::string RTSPSession::GetSdpDescription() const
{
    return sdpDescription_;
}

void RTSPSession::SetTransportInfo(const std::string &transport)
{
    transportInfo_ = transport;
}

std::string RTSPSession::GetTransportInfo() const
{
    return transportInfo_;
}

std::shared_ptr<MediaStream> RTSPSession::GetMediaStream(int track_index)
{
    if (track_index >= 0 && track_index < static_cast<int>(mediaStreams_.size())) {
        return mediaStreams_[track_index];
    }
    return nullptr;
}

const std::vector<std::shared_ptr<MediaStream>> &RTSPSession::GetMediaStreams() const
{
    return mediaStreams_;
}

bool RTSPSession::IsPlaying() const
{
    return isPlaying_;
}

bool RTSPSession::IsPaused() const
{
    return isPaused_;
}

bool RTSPSession::IsSetup() const
{
    return isSetup_;
}

void RTSPSession::UpdateLastActiveTime()
{
    lastActiveTime_ = std::time(nullptr);
}

bool RTSPSession::IsExpired(uint32_t timeout_seconds) const
{
    time_t current_time = std::time(nullptr);
    return (current_time - lastActiveTime_) > timeout_seconds;
}

time_t RTSPSession::GetLastActiveTime() const
{
    return lastActiveTime_;
}

std::string RTSPSession::GenerateSessionId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    return std::to_string(dis(gen));
}

RTPTransportParams RTSPSession::ParseTransportHeader(const std::string &transport) const
{
    RTPTransportParams params;
    // TODO: Implement transport header parsing
    return params;
}

void RTSPSession::SetMediaStreamInfo(std::shared_ptr<MediaStreamInfo> stream_info)
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    mediaStreamInfo_ = stream_info;
}

std::shared_ptr<MediaStreamInfo> RTSPSession::GetMediaStreamInfo() const
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    return mediaStreamInfo_;
}

void RTSPSession::SetRTPSender(std::shared_ptr<IRTPSender> rtp_sender)
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    rtpSender_ = rtp_sender;
}

std::shared_ptr<IRTPSender> RTSPSession::GetRTPSender() const
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    return rtpSender_;
}

bool RTSPSession::HasRTPSender() const
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    return rtpSender_ != nullptr;
}

void RTSPSession::SetRTPTransportParams(const RTPTransportParams &params)
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    rtpTransportParams_ = params;
}

RTPTransportParams RTSPSession::GetRTPTransportParams() const
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    return rtpTransportParams_;
}

bool RTSPSession::HasValidTransport() const
{
    std::lock_guard<std::mutex> lock(mediaInfoMutex_);
    // TODO: Implement transport validation logic
    return true;
}

RTPStatistics RTSPSession::GetRTPStatistics() const
{
    RTPStatistics stats;
    // TODO: Implement RTP statistics collection
    return stats;
}

} // namespace lmshao::rtsp
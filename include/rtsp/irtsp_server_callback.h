/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_IRTSP_SERVER_CALLBACK_H
#define LMSHAO_RTSP_IRTSP_SERVER_CALLBACK_H

#include <string>

namespace lmshao::rtsp {

/**
 * @brief RTSP server callback interface
 *
 * This interface defines callback methods for RTSP server to notify
 * upper layer applications about various events.
 */
class IRTSPServerCallback {
public:
    virtual ~IRTSPServerCallback() = default;

    /**
     * @brief Client connection event
     * @param client_ip Client IP address
     * @param user_agent Client User-Agent string
     */
    virtual void OnClientConnected(const std::string &client_ip, const std::string &user_agent) = 0;

    /**
     * @brief Client disconnection event
     * @param client_ip Client IP address
     */
    virtual void OnClientDisconnected(const std::string &client_ip) = 0;

    /**
     * @brief Stream request event (DESCRIBE method)
     * @param stream_path Requested stream path
     * @param client_ip Client IP address
     */
    virtual void OnStreamRequested(const std::string &stream_path, const std::string &client_ip) = 0;

    /**
     * @brief SETUP request event
     * @param client_ip Client IP address
     * @param transport Transport parameter string
     * @param stream_path Stream path
     */
    virtual void OnSetupReceived(const std::string &client_ip, const std::string &transport,
                                 const std::string &stream_path) = 0;

    /**
     * @brief PLAY request event
     * @param client_ip Client IP address
     * @param stream_path Stream path
     * @param range Play range (optional)
     */
    virtual void OnPlayReceived(const std::string &client_ip, const std::string &stream_path,
                                const std::string &range = "") = 0;

    /**
     * @brief PAUSE request event
     * @param client_ip Client IP address
     * @param stream_path Stream path
     */
    virtual void OnPauseReceived(const std::string &client_ip, const std::string &stream_path) = 0;

    /**
     * @brief TEARDOWN request event
     * @param client_ip Client IP address
     * @param stream_path Stream path
     */
    virtual void OnTeardownReceived(const std::string &client_ip, const std::string &stream_path) = 0;

    /**
     * @brief Authentication request event (optional implementation)
     * @param client_ip Client IP address
     * @param username Username
     * @param password Password
     * @return true if authentication passed, false if failed
     */
    virtual bool OnAuthenticationRequired(const std::string &client_ip, const std::string &username,
                                          const std::string &password)
    {
        return true; // Default allow all connections
    }

    /**
     * @brief Error event
     * @param client_ip Client IP address
     * @param error_code Error code
     * @param error_message Error message
     */
    virtual void OnError(const std::string &client_ip, int error_code, const std::string &error_message)
    {
        // Default empty implementation
    }
};

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_IRTSP_SERVER_CALLBACK_H
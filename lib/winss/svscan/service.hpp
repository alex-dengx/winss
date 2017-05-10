/*
 * Copyright 2016-2017 Morgan Stanley
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIB_WINSS_SVSCAN_SERVICE_HPP_
#define LIB_WINSS_SVSCAN_SERVICE_HPP_

#include <windows.h>
#include <filesystem>
#include <utility>
#include <string>
#include "easylogging/easylogging++.hpp"
#include "../windows_interface.hpp"
#include "../filesystem_interface.hpp"
#include "../handle_wrapper.hpp"
#include "service_process.hpp"

namespace fs = std::experimental::filesystem;

namespace winss {
/**
 * A template for a service.
 *
 * Models a service directory and has knowledge about redirecting logs for
 * service directories which include a log definition.
 *
 * \tparam TServiceProcess The service process implementation type.
 */
template<typename TServiceProcess>
class ServiceTmpl {
 protected:
    std::string name;  /**< The name of the service. */
    TServiceProcess main;  /**< The main supervisor. */
    TServiceProcess log;  /**< The log supervisor. */

    /**
     * Creates pipes for redirecting STDIN and STDOUT.
     *
     * \return One input pipe and one output pipe.
     */
    winss::ServicePipes CreatePipes() {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE stdin_pipe = nullptr;
        HANDLE stdout_pipe = nullptr;

        if (!WINDOWS.CreatePipe(&stdin_pipe, &stdout_pipe, &sa, 0)) {
            VLOG(1) << "CreatePipe() failed: " << ::GetLastError();
            return winss::ServicePipes{};
        }

        return winss::ServicePipes{
            winss::HandleWrapper(stdin_pipe),
            winss::HandleWrapper(stdout_pipe)
        };
    }

 public:
    static constexpr const char kLogDir[4] = "log";  /**< The log definition. */

     /**
     * The default service template constructor.
     */
    ServiceTmpl() {}

     /**
     * Initializes the service with the name and directory.
     *
     * \param name The name of the service.
     * \param service_dir The path to the service directory.
     */
    ServiceTmpl(std::string name, const fs::path& service_dir) : name(name),
        main(std::move(TServiceProcess(service_dir, false))),
        log(std::move(TServiceProcess(
            service_dir / fs::path(kLogDir), true))) {}

    ServiceTmpl(const ServiceTmpl&) = delete;  /**< No copy. */

    /**
     * Creates a new service and moves it from an old one.
     *
     * \param s The previous service.
     */
    ServiceTmpl(ServiceTmpl&& s) : name(std::move(s.name)),
        main(std::move(s.main)), log(std::move(s.log)) {}

     /**
     * Gets the name of the service
     *
     * \return The name of the service as a string.
     */
    virtual const std::string& GetName() const {
        return name;
    }

     /**
     * Resets the service.
     */
    virtual void Reset() {
        main.Reset();
        log.Reset();
    }

    /**
     * Checks the service is running.
     */
    virtual void Check() {
        winss::ServicePipes pipes;

        if (FILESYSTEM.DirectoryExists(log.GetServiceDir())) {
            VLOG(3) << "Log directory exists for service " << name;
            pipes = CreatePipes();
            log.Start(pipes);
        }

        main.Start(pipes);
    }

    /**
     * Close the service.
     *
     * \param[in] ignore_flagged Will force close the service.
     */
    virtual bool Close(bool ignore_flagged) {
        bool flagged = main.Close(ignore_flagged);
        log.Close(ignore_flagged || !flagged);

        return flagged;
    }

    void operator=(const ServiceTmpl&) = delete;  /**< No copy. */

    /**
     * Moves the service object to this object.
     *
     * \param s The previous service.
     * \return This service.
     */
    ServiceTmpl& operator=(ServiceTmpl&& s) {
        name = std::move(s.name);
        main = std::move(s.main);
        log = std::move(s.log);
        return *this;
    }
};

/**
 * Concrete service implementation.
 */
typedef ServiceTmpl<winss::ServiceProcess> Service;
}  // namespace winss

#endif  // LIB_WINSS_SVSCAN_SERVICE_HPP_

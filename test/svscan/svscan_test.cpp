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

#include <filesystem>
#include <vector>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "winss/winss.hpp"
#include "winss/handle_wrapper.hpp"
#include "winss/svscan/svscan.hpp"
#include "winss/not_owning_ptr.hpp"
#include "../mock_interface.hpp"
#include "../mock_filesystem_interface.hpp"
#include "../mock_windows_interface.hpp"
#include "../mock_wait_multiplexer.hpp"
#include "../mock_path_mutex.hpp"
#include "../mock_process.hpp"
#include "mock_service.hpp"

namespace fs = std::experimental::filesystem;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

namespace winss {
class SvScanTest : public testing::Test {
};
class HookedMockProcess : virtual public winss::NiceMockProcess {
 public:
    HookedMockProcess() : winss::Process::Process() {
        EXPECT_CALL(*this, Create(_)).WillOnce(Return(true));
        EXPECT_CALL(*this, GetHandle())
            .WillRepeatedly(Return(winss::HandleWrapper()));
    }
    HookedMockProcess(const HookedMockProcess&) = delete;
    HookedMockProcess(HookedMockProcess&& p) :
        winss::Process::Process(std::move(p)) {}

    HookedMockProcess& operator=(const HookedMockProcess&) = delete;

    HookedMockProcess& operator=(HookedMockProcess&& p) {
        winss::Process::operator=(std::move(p));
        return *this;
    }
};
class MockedSvScan : public winss::SvScanTmpl<winss::NiceMockService,
    winss::MockPathMutex, HookedMockProcess> {
 public:
    MockedSvScan(winss::NotOwningPtr<winss::WaitMultiplexer> multiplexer,
        const fs::path& scan_dir, DWORD rescan, bool signals,
        winss::EventWrapper close_event) : winss::SvScanTmpl<
        winss::NiceMockService, winss::MockPathMutex, HookedMockProcess>
        ::SvScanTmpl(multiplexer, scan_dir, rescan, signals, close_event) {}

    MockedSvScan(const MockedSvScan&) = delete;
    MockedSvScan(MockedSvScan&&) = delete;

    static void ReadEnv() {
        return winss::SvScanTmpl<winss::NiceMockService,
            winss::MockPathMutex, HookedMockProcess>::ReadEnv();
    }

    std::vector<winss::NiceMockService>* GetServices() {
        return &services;
    }

    winss::MockPathMutex* GetMutex() {
        return &mutex;
    }

    MockedSvScan& operator=(const MockedSvScan&) = delete;
    MockedSvScan& operator=(MockedSvScan&&) = delete;
};

TEST_F(SvScanTest, Init) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 0, false,
        close_event);

    EXPECT_CALL(*file, ChangeDirectory(_))
        .WillOnce(Return(true));

    EXPECT_CALL(*file, GetDirectories(_))
        .WillOnce(Return(std::vector<fs::path>()));

    EXPECT_CALL(*file, Read(_)).WillOnce(Return(""));
    EXPECT_CALL(*file, GetFiles(_)).WillOnce(Return(std::vector<fs::path>()));

    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillOnce(Return(false))
        .WillOnce(Return(true))
        .WillOnce(Return(true));

    EXPECT_CALL(*svscan.GetMutex(), Lock())
        .WillOnce(Return(true));

    multiplexer.mock_init_callbacks.at(0)(multiplexer);
    multiplexer.mock_init_callbacks.at(0)(multiplexer);
}


TEST_F(SvScanTest, ReadEnv) {
    MockInterface<winss::MockWindowsInterface> windows;
    MockInterface<winss::MockFilesystemInterface> file;

    EXPECT_CALL(*file, Read(_))
        .WillOnce(Return(""))
        .WillOnce(Return("value"))
        .WillOnce(Return(""));
    EXPECT_CALL(*file, GetFiles(_))
        .WillOnce(Return(std::vector<fs::path>{
            fs::path("test1"), fs::path("test2")
        }));

    EXPECT_CALL(*windows, SetEnvironmentVariable(
        StrEq("test1"), StrEq("value")));
    EXPECT_CALL(*windows, SetEnvironmentVariable(StrEq("test2"), nullptr));

    MockedSvScan::ReadEnv();
}

TEST_F(SvScanTest, InitDirNotExists) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 0, false,
        close_event);

    EXPECT_CALL(multiplexer, Stop(_)).Times(1);

    EXPECT_CALL(*file, ChangeDirectory(_))
        .WillOnce(Return(false));

    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillOnce(Return(false));

    multiplexer.mock_init_callbacks.at(0)(multiplexer);
}

TEST_F(SvScanTest, InitLockTaken) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 0, false,
        close_event);

    EXPECT_CALL(multiplexer, Stop(_)).Times(1);

    EXPECT_CALL(*file, ChangeDirectory(_))
        .WillOnce(Return(true));

    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillOnce(Return(false));

    EXPECT_CALL(*svscan.GetMutex(), Lock())
        .WillOnce(Return(false));

    multiplexer.mock_init_callbacks.at(0)(multiplexer);
}

TEST_F(SvScanTest, Scan) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 0, false,
        close_event);

    EXPECT_CALL(*file, GetDirectories(_))
        .WillOnce(Return(std::vector<fs::path>({
        ".", "..", ".hidden", "test1", "test2"
    })));

    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillOnce(Return(true));

    svscan.Scan(false);

    EXPECT_EQ(2, svscan.GetServices()->size());
}

TEST_F(SvScanTest, ReScan) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 5000, false,
        close_event);

    EXPECT_CALL(*file, GetDirectories(_))
        .WillOnce(Return(std::vector<fs::path>({".", "..", ".hidden",
            "test1", "test2"})))
        .WillOnce(Return(std::vector<fs::path>({".", "..", ".hidden",
            "test1", "test2", "test3" })));

    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillOnce(Return(true))
        .WillOnce(Return(true));

    svscan.Scan(false);

    ASSERT_EQ(2, svscan.GetServices()->size());

    EXPECT_CALL(svscan.GetServices()->at(0), Check()).Times(1);
    EXPECT_CALL(svscan.GetServices()->at(1), Check()).Times(1);

    multiplexer.mock_timeout_callbacks.at(0)(multiplexer);

    EXPECT_EQ(3, svscan.GetServices()->size());
}

TEST_F(SvScanTest, CloseAllServices) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 5000, false,
        close_event);

    EXPECT_CALL(*file, GetDirectories(_))
        .WillOnce(Return(std::vector<fs::path>({ ".", "..", ".hidden",
            "test1", "test2" })));

    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillRepeatedly(Return(true));

    svscan.Scan(false);

    ASSERT_EQ(2, svscan.GetServices()->size());

    EXPECT_CALL(svscan.GetServices()->at(0), Close(false))
        .WillOnce(Return(true));
    EXPECT_CALL(svscan.GetServices()->at(1), Close(false))
        .WillOnce(Return(false));

    svscan.CloseAllServices(false);

    EXPECT_EQ(1, svscan.GetServices()->size());

    EXPECT_CALL(svscan.GetServices()->at(0), Close(true))
        .WillOnce(Return(false));

    multiplexer.mock_stop_callbacks.at(0)(multiplexer);

    EXPECT_EQ(0, svscan.GetServices()->size());
}

TEST_F(SvScanTest, Finish) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 0, false,
        close_event);

    EXPECT_CALL(multiplexer, AddTriggeredCallback(_, _)).Times(1);
    EXPECT_CALL(*file, Read(_)).WillOnce(Return("cmd"));

    svscan.Exit(false);
    multiplexer.mock_stop_callbacks.at(0)(multiplexer);
}

TEST_F(SvScanTest, SignalsDiverted) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 0, true,
        close_event);

    EXPECT_CALL(*file, Read(_)).WillOnce(Return("cmd"));
    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillRepeatedly(Return(true));

    multiplexer.mock_init_callbacks.at(0)(multiplexer);

    multiplexer.mock_triggered_callbacks.at(0)(multiplexer,
        close_event.GetHandle());
}

TEST_F(SvScanTest, SignalsNotDiverted) {
    MockInterface<winss::MockFilesystemInterface> file;
    NiceMock<winss::MockWaitMultiplexer> multiplexer;
    winss::EventWrapper close_event;
    MockedSvScan svscan(winss::NotOwned(&multiplexer), ".", 0, false,
        close_event);

    EXPECT_CALL(*svscan.GetMutex(), HasLock())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(multiplexer, Stop(_)).Times(1);

    multiplexer.mock_init_callbacks.at(0)(multiplexer);

    multiplexer.mock_triggered_callbacks.at(0)(multiplexer,
        close_event.GetHandle());
}
}  // namespace winss

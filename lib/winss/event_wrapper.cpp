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

#include "event_wrapper.hpp"
#include "windows_interface.hpp"
#include "handle_wrapper.hpp"

winss::EventWrapper::EventWrapper() {
    handle = WINDOWS.CreateEvent(nullptr, true, false, nullptr);
}

bool winss::EventWrapper::IsSet() const {
    return WINDOWS.WaitForSingleObject(handle, 0) != WAIT_TIMEOUT;
}

bool winss::EventWrapper::Set() {
    return WINDOWS.SetEvent(handle);
}

winss::HandleWrapper winss::EventWrapper::GetHandle() const {
    return winss::HandleWrapper(handle, false, SYNCHRONIZE);
}

winss::EventWrapper::~EventWrapper() {
    WINDOWS.CloseHandle(handle);
}

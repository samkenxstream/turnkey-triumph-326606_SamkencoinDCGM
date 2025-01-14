/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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
/*
 * File:   TestDiagResponseWrapper.h
 */

#ifndef TEST_DIAG_RESPONSE_WRAPPER_H
#define TEST_DIAG_RESPONSE_WRAPPER_H

#include "TestDcgmModule.h"
#include "dcgm_structs.h"

class TestDiagResponseWrapper : public TestDcgmModule
{
public:
    TestDiagResponseWrapper();
    ~TestDiagResponseWrapper() override;

    /*************************************************************************/
    /* Inherited methods from TestDcgmModule */
    int Init(std::vector<std::string> argv, std::vector<test_nvcm_gpu_t> gpus) override;
    int Run() override;
    int Cleanup() override;
    std::string GetTag() override;

private:
    /*************************************************************************/
    /**
     * Actual test cases. These should return a status like below
     *
     * Returns 0 on success
     *        <0 on fatal error. Will abort entire framework
     *        >0 on non-fatal error
     *
     */
    static int TestInitializeDiagResponse();
    static int TestSetPerGpuResponseState();
    static int TestAddPerGpuMessage();
    static int TestSetGpuIndex();
    static int TestGetBasicTestResultIndex();
    static int TestRecordSystemError();
    static int TestAddErrorDetail();
};

#endif /* TEST_DIAG_RESPONSE_WRAPPER_H */

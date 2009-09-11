/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#ifndef TEST_ENGINE_UTILITY_H_HEADER
#define TEST_ENGINE_UTILITY_H_HEADER

#include "test_case.h"
#include "xml_test_interpreter.h"
#include "oscl_file_io.h"
#define MAX_324_TEST 100000

void FindTestRange(cmd_line* command_line,
                   int32 &iFirstTest,
                   int32 &iLastTest,
                   FILE* aFile);

void FindXmlResultsFile(cmd_line* command_line,
                        OSCL_HeapString<OsclMemAllocator> &XmlTestResultsFilename,
                        FILE *aFile);

void WriteInitialXmlSummary(OSCL_HeapString<OsclMemAllocator> &xmlresultsfile,
                            FILE * aFile);

void WriteFinalXmlSummary(OSCL_HeapString<OsclMemAllocator> &xmlresultsfile,
                          const test_result& result,
                          FILE *aFile);

#endif

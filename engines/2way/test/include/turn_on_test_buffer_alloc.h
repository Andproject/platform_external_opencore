/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
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
#ifndef TURN_ON_TEST_BUFFER_ALLOC_H_INCLUDED
#define TURN_ON_TEST_BUFFER_ALLOC_H_INCLUDED

#include "av_using_test_extension.h"




class turn_on_test_buffer_alloc : public av_using_test_extension
{
    public:
        turn_on_test_buffer_alloc(bool aUseProxy = false,
                                  uint32 aTimeConnection = TEST_DURATION,
                                  uint32 aMaxTestDuration = MAX_TEST_DURATION)
                : av_using_test_extension(aUseProxy, aTimeConnection, aMaxTestDuration)

        {
            iTestName = _STRLIT_CHAR("Use external buffer allocator for decoder nodes");

        }

        ~turn_on_test_buffer_alloc()
        {
        }

    private:

        void FinishTimerCallback();


};


#endif



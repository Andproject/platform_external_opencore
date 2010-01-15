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
#include "pv_2way_media.h"
#include "oscl_utf8conv.h"
#include "oscl_thread.h"
#include "oscl_error.h"
#include "oscl_socket.h"
#include "oscl_timer.h"
#include "oscl_mem.h"


#define EXTN_LEN       5
#define OUTPUT_MIO_SIZE 300

void ConvertToLowerCase(char *aString)
{
    int32 counter = 0;
    while (aString[counter])
    {
        // convert the input character to lower case
        aString[counter] = oscl_tolower(aString[counter]);
        counter++;
    }
}


PVMFFormatType PV2WayMedia::GetMediaFormat(const oscl_wchar* aFileName)
{
    PVMFFormatType inputFileFormatType = PVMF_MIME_FORMAT_UNKNOWN;

    int32 fileLen = oscl_strlen(aFileName);
    char *file = (char *) OSCL_MALLOC(fileLen + 1);
    char extn[EXTN_LEN];

    oscl_UnicodeToUTF8(aFileName, oscl_strlen(aFileName), file, fileLen + 1);

    int len = oscl_strlen(file);

    int filenameLen = len;
    char *tempfile = file + len; //Move pointer to end of string

    int counter = 0;
    filenameLen = len;
    while (filenameLen > 0 && file[filenameLen] != '.' && file[filenameLen] != '\\')
    {
        tempfile--;
        filenameLen--;
        counter++;
    }

    if (counter > 0)
    {
        oscl_strncpy(extn, tempfile, counter + 1);
    }

    ConvertToLowerCase(extn);
    uint32 count = oscl_strlen(extn);

    if (oscl_strncmp(extn, ".if2", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_AMR_IF2;

    }
    else if (oscl_strncmp(extn, ".ietf", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_AMR_IETF;

    }
    else if (oscl_strncmp(extn, ".amr", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_AMRWB_IETF;

    }
    else if (oscl_strncmp(extn, ".m4v", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_M4V;

    }
    else if (oscl_strncmp(extn, ".h263", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_H2632000;

    }
    else if (oscl_strncmp(extn, ".h264", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_H264_VIDEO_RAW;
    }
    else if (oscl_strncmp(extn, ".yuv420", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_YUV420;
    }
    else if (oscl_strncmp(extn, ".yuv422", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_YUV422;
    }
    else if (oscl_strncmp(extn, ".pcm16", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_PCM16;
    }
    else if (oscl_strncmp(extn, ".pcm8", count) == 0)
    {
        inputFileFormatType = PVMF_MIME_PCM8;
    }
    else
    {
        ConvertToLowerCase(file);

        if (oscl_strstr(file, "yuv420") != 0)
        {
            inputFileFormatType = PVMF_MIME_YUV420;
        }
        else if (oscl_strstr(file, "yuv422") != 0)
        {
            inputFileFormatType = PVMF_MIME_YUV422;
        }
        else if (oscl_strstr(file, "pcm16") != 0)
        {
            inputFileFormatType = PVMF_MIME_PCM16;
        }
        else if (oscl_strstr(file, "pcm8") != 0)
        {
            inputFileFormatType = PVMF_MIME_PCM8;
        }
    }

    OSCL_FREE(file);
    return inputFileFormatType;
}


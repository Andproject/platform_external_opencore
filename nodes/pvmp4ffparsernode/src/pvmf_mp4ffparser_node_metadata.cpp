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
#include "pvmf_mp4ffparser_node.h"

#include "impeg4file.h"


#include "media_clock_converter.h"

#include "pv_mime_string_utils.h"

#include "oscl_snprintf.h"

#include "pvmf_duration_infomessage.h"

#include "pvmi_kvp_util.h"

#include "h263decoderspecificinfo.h"

#include "oscl_exclusive_ptr.h"

#include "getactualaacconfig.h"

#define NUMMETADATAKEYS 64


uint32 PVMFMP4FFParserNode::GetNumMetadataKeys(char* aQueryKeyString)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetNumMetadataKeys() called"));

    uint32 num_entries = 0;
    if (aQueryKeyString == NULL)
    {
        // No query key so just return all the available keys
        num_entries = iAvailableMetadataKeys.size();
    }
    else
    {
        // Determine the number of metadata keys based on the query key string provided
        for (uint32 i = 0; i < iAvailableMetadataKeys.size(); i++)
        {
            // Check if the key matches the query key
            if (pv_mime_strcmp(iAvailableMetadataKeys[i].get_cstr(), aQueryKeyString) >= 0)
            {
                num_entries++;
            }
        }
    }

    if ((iCPMMetaDataExtensionInterface != NULL) &&
            (iProtectedFile == true))
    {
        num_entries +=
            iCPMMetaDataExtensionInterface->GetNumMetadataKeys(aQueryKeyString);
    }

    return num_entries;

}

uint32 PVMFMP4FFParserNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetNumMetadataValues() called"));
    uint32 numvalentries = 0;
    if (iMP4FileHandle)
    {
        numvalentries = iMP4FileHandle->GetNumMetadataValues(aKeyList);
    }

    if (iCPMMetaDataExtensionInterface != NULL)
    {
        numvalentries +=
            iCPMMetaDataExtensionInterface->GetNumMetadataValues(aKeyList);
    }
    return numvalentries;
}


PVMFCommandId PVMFMP4FFParserNode::GetNodeMetadataKeys(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, uint32 starting_index, int32 max_entries, char* query_key, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetNodeMetadataKeys() called"));

    PVMFMP4FFParserNodeCommand cmd;
    cmd.PVMFMP4FFParserNodeCommand::Construct(aSessionId, PVMP4FF_NODE_CMD_GETNODEMETADATAKEYS, aKeyList, starting_index, max_entries, query_key, aContext);
    return QueueCommandL(cmd);
}


PVMFCommandId PVMFMP4FFParserNode::GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 starting_index, int32 max_entries, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::GetNodeMetadataValue() called"));

    PVMFMP4FFParserNodeCommand cmd;
    cmd.PVMFMP4FFParserNodeCommand::Construct(aSessionId, PVMP4FF_NODE_CMD_GETNODEMETADATAVALUES, aKeyList, aValueList, starting_index, max_entries, aContext);
    return QueueCommandL(cmd);
}


PVMFStatus PVMFMP4FFParserNode::ReleaseNodeMetadataKeys(PVMFMetadataList& , uint32 , uint32)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::ReleaseNodeMetadataKeys() called"));

    // Nothing needed-- there's no dynamic allocation in this node's key list
    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList,
        uint32 start,
        uint32 end)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::ReleaseNodeMetadataValues() called"));

    if (iMP4FileHandle == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFMP4FFParserNode::ReleaseNodeMetadataValues() \
                       MP4 file not parsed yet"));
        return PVMFFailure;
    }

    end = OSCL_MIN(aValueList.size(), iMP4ParserNodeMetadataValueCount);

    if (start > end || aValueList.size() == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFMP4FFParserNode::ReleaseNodeMetadataValues() \
                        Invalid start/end index"));
        return PVMFErrArgument;
    }

    //First few entries in value list have ID3 specific data. We do not have to release that data
    //as it will be released by ID3Parcom. Hence we need to modify the start value if it is less than
    //iTotalID3MetaDataTagInValueList
    if (start < iTotalID3MetaDataTagInValueList)
    {
        start = iTotalID3MetaDataTagInValueList;
    }

    // Go through the specified values and free it
    for (uint32 i = start; i < end; i++)
    {
        iMP4FileHandle->ReleaseMetadataValue(aValueList[i]);
    }
    return PVMFSuccess;
}



PVMFStatus
PVMFMP4FFParserNode::DoGetMetadataKeys(PVMFMP4FFParserNodeCommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::DoGetMetadataKeys() In"));
    /* Get Metadata keys from CPM for protected content only */
    if ((iCPMMetaDataExtensionInterface != NULL) &&
            (iProtectedFile == true))
    {
        GetCPMMetaDataKeys();
        return PVMFPending;
    }
    if (iMP4FileHandle == NULL)
    {
        return PVMFErrInvalidState;
    }
    return (CompleteGetMetadataKeys(aCmd));
}

PVMFStatus PVMFMP4FFParserNode::CompleteGetMetadataKeys(PVMFMP4FFParserNodeCommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFMP4FFParserNode::CompleteGetMetadataKeys() In"));

    PVMFMetadataList* keylistptr = NULL;
    uint32 starting_index;
    int32 max_entries;
    char* query_key = NULL;

    aCmd.PVMFMP4FFParserNodeCommand::Parse(keylistptr, starting_index, max_entries, query_key);

    // Check parameters
    if (keylistptr == NULL)
    {
        // The list pointer is invalid
        return PVMFErrArgument;
    }

    // Copy the requested keys
    uint32 num_entries = 0;
    int32 num_added = 0;
    uint32 lcv = 0;
    for (lcv = 0; lcv < iAvailableMetadataKeys.size(); lcv++)
    {
        if (query_key == NULL)
        {
            // No query key so this key is counted
            ++num_entries;
            if (num_entries > starting_index)
            {
                // Past the starting index so copy the key
                PVMFStatus status = PushValueToList(iAvailableMetadataKeys, keylistptr, lcv);
                if (PVMFErrNoMemory == status)
                {
                    return status;
                }
                num_added++;
            }
        }
        else
        {
            // Check if the key matches the query key
            if (oscl_strstr(iAvailableMetadataKeys[lcv].get_cstr(), query_key) != NULL)
            {
                // This key is counted
                ++num_entries;
                if (num_entries > starting_index)
                {
                    // Past the starting index so copy the key
                    PVMFStatus status = PushValueToList(iAvailableMetadataKeys, keylistptr, lcv);
                    if (PVMFErrNoMemory == status)
                    {
                        return status;
                    }
                    num_added++;
                }
            }
        }

        // Check if max number of entries have been copied
        if (max_entries > 0 && num_added >= max_entries)
        {
            break;
        }
    }

    for (lcv = 0; lcv < iCPMMetadataKeys.size(); lcv++)
    {
        if (query_key == NULL)
        {
            /* No query key so this key is counted */
            ++num_entries;
            if (num_entries > starting_index)
            {
                /* Past the starting index so copy the key */
                PVMFStatus status = PushValueToList(iCPMMetadataKeys, keylistptr, lcv);
                if (PVMFErrNoMemory == status)
                {
                    return status;
                }
                num_added++;
            }
        }
        else
        {
            /* Check if the key matches the query key */
            if (pv_mime_strcmp(iCPMMetadataKeys[lcv].get_cstr(), query_key) >= 0)
            {
                /* This key is counted */
                ++num_entries;
                if (num_entries > starting_index)
                {
                    /* Past the starting index so copy the key */
                    PVMFStatus status = PushValueToList(iCPMMetadataKeys, keylistptr, lcv);
                    if (PVMFErrNoMemory == status)
                    {
                        return status;
                    }
                    num_added++;
                }
            }
        }
        /* Check if max number of entries have been copied */
        if ((max_entries > 0) && (num_added >= max_entries))
        {
            break;
        }
    }

    return PVMFSuccess;
}


PVMFStatus PVMFMP4FFParserNode::DoGetMetadataValues(PVMFMP4FFParserNodeCommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFMP4FFParserNode::DoGetMetadataValues() In"));

    PVMFMetadataList* keylistptr_in = NULL;
    PVMFMetadataList* keylistptr = NULL;
    PVMFMetadataList completeKeyList;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    int32 numentriesadded = 0;
    uint32 numvalentries = 0;

    // Extract parameters from command structure
    aCmd.PVMFMP4FFParserNodeCommand::Parse(keylistptr_in,
                                           valuelistptr,
                                           starting_index,
                                           max_entries);

    if (iMP4FileHandle == NULL || keylistptr_in == NULL || valuelistptr == NULL)
    {
        // The list pointer is invalid, or we cannot access the mp4 ff library.
        return PVMFFailure;
    }

    keylistptr = keylistptr_in;

    //If numkeys is one, just check to see if the request
    //is for ALL metadata
    if (keylistptr_in->size() == 1)
    {
        if (oscl_strncmp((*keylistptr)[0].get_cstr(),
                         PVMP4_ALL_METADATA_KEY,
                         oscl_strlen(PVMP4_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iAvailableMetadataKeys;
        }
    }

    uint32 numKeys = keylistptr->size();

    // The underlying mp4 ff library will fill in the values.
    iTotalID3MetaDataTagInValueList = 0;
    PVMFStatus status = iMP4FileHandle->GetMetadataValues(*keylistptr, *valuelistptr,
                        starting_index, max_entries,
                        numentriesadded,
                        iTotalID3MetaDataTagInValueList);

    numvalentries = numvalentries + numentriesadded;

    if (numvalentries >= (uint32)max_entries)
    {
        //If required number of entries have already been added
        iMP4ParserNodeMetadataValueCount = (*valuelistptr).size();
        return PVMFSuccess;
    }

    uint32 lcv = 0;
    // Retrieve the track ID list
    OsclExclusiveArrayPtr<uint32> trackidlistexclusiveptr;
    uint32* trackidlist = NULL;
    uint32 numTracks = (uint32)(iMP4FileHandle->getNumTracks());
    status = CreateNewArray(&trackidlist, numTracks);
    if (PVMFErrNoMemory == status)
    {
        return PVMFErrNoMemory;
    }
    oscl_memset(trackidlist, 0, sizeof(uint32)*(numTracks));
    iMP4FileHandle->getTrackIDList(trackidlist, numTracks);
    trackidlistexclusiveptr.set(trackidlist);

    for (lcv = 0; lcv < numKeys; lcv++)
    {
        int32 leavecode = 0;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;
        KeyVal.value.pWChar_value = NULL;
        KeyVal.value.pChar_value = NULL;

        bool IsMetadataValAddedBefore = false;

        if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SELECTED_KEY) != NULL)
        {

            // Track selected info

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMP4FileHandle->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                PVMFStatus retval = PVMFErrArgument;

                // Increment the counter for the number of values found so far
                ++numvalentries;
                // Add the value entry if past the starting index
                if (numvalentries > starting_index)
                {
                    char indexparam[16];
                    oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                    indexparam[15] = '\0';

                    // Check if the track has been selected by looking up
                    // the current index's track ID in the NodeTrackPort vector
                    bool trackselected = false;
                    for (uint32 j = 0; j < iNodeTrackPortList.size(); ++j)
                    {
                        if ((uint32)iNodeTrackPortList[j].iTrackId == trackidlist[i])
                        {
                            trackselected = true;
                            break;
                        }
                    }
                    retval = PVMFCreateKVPUtils::CreateKVPForBoolValue(trackkvp, PVMP4METADATA_TRACKINFO_SELECTED_KEY, trackselected, indexparam);

                    if (retval != PVMFSuccess && retval != PVMFErrArgument)
                    {
                        break;
                    }

                    if (trackkvp.key != NULL)
                    {
                        leavecode = AddToValueList(*valuelistptr, trackkvp);
                        if (leavecode != 0)
                        {
                            OSCL_ARRAY_DELETE(trackkvp.key);
                            trackkvp.key = NULL;
                        }
                        else
                        {
                            // Increment the value list entry counter
                            ++numentriesadded;
                            IsMetadataValAddedBefore = true;
                        }

                        // Check if the max number of value entries were added
                        if (max_entries > 0 && numentriesadded >= max_entries)
                        {

                            return PVMFSuccess;
                        }
                    }
                }
            }
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_RANDOM_ACCESS_DENIED_KEY) == 0)
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            /* Create a value entry if past the starting index */
            if (numvalentries > (uint32)starting_index)
            {
                uint64 duration64 = iMP4FileHandle->getMovieDuration();
                uint32 duration = Oscl_Int64_Utils::get_uint64_lower32(duration64);
                bool random_access_denied = false;
                if (duration > 0)
                {
                    random_access_denied = false;
                }
                else
                {
                    random_access_denied = true;
                }

                if (iMP4FileHandle->IsMovieFragmentsPresent())
                {
                    if (iDataStreamInterface != NULL)
                        random_access_denied = true;

                    uint32* trackList = NULL;
                    uint32 numTracks = iNodeTrackPortList.size();
                    CreateNewArray(&trackList, numTracks);
                    if (trackList)
                    {
                        for (uint32 i = 0; i < iNodeTrackPortList.size(); i++)
                        {
                            // Save the track list while in this loop
                            trackList[i] = iNodeTrackPortList[i].iTrackId;
                        }

                        if (!iMP4FileHandle->IsTFRAPresentForAllTrack(numTracks, trackList))
                            random_access_denied = true;

                        OSCL_ARRAY_DELETE(trackList);
                    }
                }

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                            PVMP4METADATA_RANDOM_ACCESS_DENIED_KEY,
                            random_access_denied,
                            NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_CLIP_TYPE_KEY) == 0)
        {
            // clip-type
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 len = 0;
                char* clipType = NULL;
                if (download_progress_interface != NULL)
                {
                    len = oscl_strlen("download");
                    clipType = OSCL_ARRAY_NEW(char, len + 1);
                    oscl_memset(clipType, 0, len + 1);
                    oscl_strncpy(clipType, ("download"), len);
                }
                else
                {
                    len = oscl_strlen("local");
                    clipType = OSCL_ARRAY_NEW(char, len + 1);
                    oscl_memset(clipType, 0, len + 1);
                    oscl_strncpy(clipType, ("local"), len);
                }

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                            PVMP4METADATA_CLIP_TYPE_KEY,
                            clipType);

                OSCL_ARRAY_DELETE(clipType);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_WIDTH_KEY) != NULL)
        {
            // Video track width

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMP4FileHandle->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMP4FileHandle->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_VISUAL)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 trackwidth = (uint32)(FindVideoDisplayWidth(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_VIDEO_WIDTH_KEY, trackwidth, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {

                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_VIDEO_HEIGHT_KEY) != NULL)
        {
            // Video track height

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMP4FileHandle->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMP4FileHandle->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_VISUAL)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 trackheight = (uint32)(FindVideoDisplayHeight(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_VIDEO_HEIGHT_KEY, trackheight, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {

                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_SAMPLERATE_KEY) != NULL)
        {
            // Sampling rate (only for video tracks)

            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMP4FileHandle->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMP4FileHandle->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_AUDIO)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 samplerate = GetAudioSampleRate(trackidlist[i]);
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_SAMPLERATE_KEY, samplerate, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {

                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_NUMCHANNELS_KEY) != NULL)
        {
            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMP4FileHandle->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMP4FileHandle->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_AUDIO)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 numAudioChannels = (GetNumAudioChannels(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_AUDIO_NUMCHANNELS_KEY, numAudioChannels, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {

                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }
        else if (oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_TRACKINFO_AUDIO_BITS_PER_SAMPLE_KEY) != NULL)
        {
            // Determine the index requested. Default to all tracks
            // Check if the file has at least one track
            int32 numtracks = iMP4FileHandle->getNumTracks();
            if (numtracks <= 0)
            {
                break;
            }
            uint32 startindex = 0;
            uint32 endindex = (uint32)numtracks - 1;
            // Check if the index parameter is present
            const char* indexstr = oscl_strstr((*keylistptr)[lcv].get_cstr(), PVMP4METADATA_INDEX);
            if (indexstr != NULL)
            {
                // Retrieve the index values
                GetIndexParamValues(indexstr, startindex, endindex);
            }
            // Validate the indices
            if (startindex > endindex || startindex >= (uint32)numtracks || endindex >= (uint32)numtracks)
            {
                break;
            }

            // Return a KVP for each index
            for (uint32 i = startindex; i <= endindex; ++i)
            {
                PvmiKvp trackkvp;
                trackkvp.key = NULL;

                if (iMP4FileHandle->getTrackMediaType(trackidlist[i]) == MEDIA_TYPE_AUDIO)
                {
                    // Increment the counter for the number of values found so far
                    numvalentries++;

                    // Add the value entry if past the starting index
                    if (numvalentries > starting_index)
                    {
                        char indexparam[16];
                        oscl_snprintf(indexparam, 16, ";%s%d", PVMP4METADATA_INDEX, i);
                        indexparam[15] = '\0';

                        uint32 numbitspersample = (GetAudioBitsPerSample(trackidlist[i]));
                        PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(trackkvp, PVMP4METADATA_TRACKINFO_AUDIO_BITS_PER_SAMPLE_KEY, numbitspersample, indexparam);
                        if (retval != PVMFSuccess && retval != PVMFErrArgument)
                        {
                            break;
                        }

                        if (trackkvp.key != NULL)
                        {
                            leavecode = AddToValueList(*valuelistptr, trackkvp);
                            if (leavecode != 0)
                            {
                                OSCL_ARRAY_DELETE(trackkvp.key);
                                trackkvp.key = NULL;
                            }
                            else
                            {
                                // Increment the value list entry counter
                                ++numentriesadded;
                                IsMetadataValAddedBefore = true;
                            }

                            // Check if the max number of value entries were added
                            if (max_entries > 0 && numentriesadded >= max_entries)
                            {

                                return PVMFSuccess;
                            }
                        }
                    }
                }
            }
        }

        // Add the KVP to the list if the key string was created
        if ((KeyVal.key != NULL) && (!IsMetadataValAddedBefore))
        {
            leavecode = AddToValueList(*valuelistptr, KeyVal);
            if (leavecode != 0)
            {
                switch (GetValTypeFromKeyString(KeyVal.key))
                {
                    case PVMI_KVPVALTYPE_CHARPTR:
                        if (KeyVal.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                            KeyVal.value.pChar_value = NULL;
                        }
                        break;

                    case PVMI_KVPVALTYPE_WCHARPTR:
                        if (KeyVal.value.pWChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pWChar_value);
                            KeyVal.value.pWChar_value = NULL;
                        }
                        break;

                    default:
                        // Add more case statements if other value types are returned
                        break;
                }

                OSCL_ARRAY_DELETE(KeyVal.key);
                KeyVal.key = NULL;
            }
            else
            {
                // Increment the counter for number of value entries added to the list
                ++numentriesadded;
            }

            // Check if the max number of value entries were added
            if (max_entries > 0 && numentriesadded >= max_entries)
            {
                // Maximum number of values added so break out of the loop
                //return PVMFSuccess;
                break;
            }
        }
    }

    iMP4ParserNodeMetadataValueCount = (*valuelistptr).size();

    if (iCPMMetaDataExtensionInterface != NULL)
    {
        iCPMGetMetaDataValuesCmdId =
            (iCPMMetaDataExtensionInterface)->GetNodeMetadataValues(iCPMSessionID,
                    (*keylistptr_in),
                    (*valuelistptr),
                    0);
        return PVMFPending;
    }
    return status;
}



void PVMFMP4FFParserNode::CompleteGetMetaDataValues()
{
    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    iCurrentCommand.front().PVMFMP4FFParserNodeCommand::Parse(keylistptr, valuelistptr, starting_index, max_entries);

    for (uint32 i = 0; i < (*valuelistptr).size(); i++)
    {
        PVMF_MP4FFPARSERNODE_LOGINFO((0, "PVMFMP4FFParserNode::CompleteGetMetaDataValues - Index=%d, Key=%s", i, (*valuelistptr)[i].key));
    }

    CommandComplete(iCurrentCommand,
                    iCurrentCommand.front(),
                    PVMFSuccess);
}

int32 PVMFMP4FFParserNode::AddToValueList(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, PvmiKvp& aNewValue)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aValueList.push_back(aNewValue));
    return leavecode;
}


void PVMFMP4FFParserNode::CreateDurationInfoMsg(uint32 adurationms)
{
    int32 leavecode = 0;
    PVMFDurationInfoMessage* eventmsg = NULL;
    OSCL_TRY(leavecode, eventmsg = OSCL_NEW(PVMFDurationInfoMessage, (adurationms)));
    PVMFNodeInterface::ReportInfoEvent(PVMFInfoDurationAvailable, NULL, OSCL_STATIC_CAST(PVInterface*, eventmsg));
    if (eventmsg)
    {
        eventmsg->removeRef();
    }
}


PVMFStatus PVMFMP4FFParserNode::PushValueToList(Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> &aRefMetaDataKeys, PVMFMetadataList *&aKeyListPtr, uint32 aLcv)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aKeyListPtr->push_back(aRefMetaDataKeys[aLcv]));
    OSCL_FIRST_CATCH_ANY(leavecode, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFMP4FFParserNode::PushValueToList() Memory allocation failure when copying metadata key")); return PVMFErrNoMemory);
    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::InitMetaData()
{
    if (iMP4FileHandle)
    {
        int32 leavecode = 0;
        OSCL_TRY(leavecode, iAvailableMetadataKeys.reserve(NUMMETADATAKEYS));
        iMP4FileHandle->InitMetaData(&iAvailableMetadataKeys);
    }

    int32 iNumTracks = iMP4FileHandle->getNumTracks();
    uint32 iIdList[16];

    if (iNumTracks != iMP4FileHandle->getTrackIDList(iIdList, iNumTracks))
    {
        return PVMFFailure;
    }
    for (int32 i = iNumTracks - 1; i >= 0; i--)
    {
        uint32 trackID = iIdList[i];

        OSCL_HeapString<OsclMemAllocator> trackMIMEType;

        iMP4FileHandle->getTrackMIMEType(trackID, (OSCL_String&)trackMIMEType);

        if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_M4V, oscl_strlen(PVMF_MIME_M4V)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H2632000, oscl_strlen(PVMF_MIME_H2632000)) == 0) ||
                (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_H264_VIDEO_MP4, oscl_strlen(PVMF_MIME_H264_VIDEO_MP4)) == 0))
        {
            if (PVMFSuccess == PopulateVideoDimensions(trackID))
            {
                //track id is a one based index
                char indexparam[18];
                oscl_snprintf(indexparam, 18, ";index=%d", i);
                indexparam[17] = '\0';

                PushToAvailableMetadataKeysList(PVMP4METADATA_TRACKINFO_VIDEO_WIDTH_KEY, indexparam);
                PushToAvailableMetadataKeysList(PVMP4METADATA_TRACKINFO_VIDEO_HEIGHT_KEY, indexparam);
            }
        }
    }

    if (iMP4FileHandle->getMovieDuration() > (uint64)0)
    {
        // Intimate the Duration info available to the engine through Informational Event.
        uint64 duration64 = iMP4FileHandle->getMovieDuration();
        uint32 durationms = 0;
        uint32 duration = durationms = Oscl_Int64_Utils::get_uint64_lower32(duration64);
        uint32 timescale = iMP4FileHandle->getMovieTimescale();
        if (timescale > 0 && timescale != 1000)
        {
            // Convert to milliseconds
            MediaClockConverter mcc(timescale, duration);
            durationms = mcc.get_converted_ts(1000);
        }
        CreateDurationInfoMsg(durationms);
    }

    //set clip duration on download progress interface
    //applicable to PDL sessions
    {
        if (iMP4FileHandle != NULL)
        {
            MediaClockConverter mcc(iMP4FileHandle->getMovieTimescale());
            uint32 movieduration =
                Oscl_Int64_Utils::get_uint64_lower32(iMP4FileHandle->getMovieDuration());
            mcc.update_clock(movieduration);
            uint32 moviedurationInMS = mcc.get_converted_ts(1000);
            if ((download_progress_interface != NULL) && (moviedurationInMS != 0))
            {
                download_progress_interface->setClipDuration(OSCL_CONST_CAST(uint32, moviedurationInMS));
            }
        }
    }

    return PVMFSuccess;

}

void PVMFMP4FFParserNode::PushToAvailableMetadataKeysList(const char* aKeystr, char* aOptionalParam)
{
    if (aKeystr == NULL)
    {
        return;
    }

    if (aOptionalParam)
    {
        iAvailableMetadataKeys.push_front(aKeystr);
        iAvailableMetadataKeys[0] += aOptionalParam;
    }

    else
    {
        iAvailableMetadataKeys.push_front(aKeystr);
    }
}

PVMFStatus PVMFMP4FFParserNode::GetIndexParamValues(const char* aString, uint32& aStartIndex, uint32& aEndIndex)
{
    // This parses a string of the form "index=N1...N2" and extracts the integers N1 and N2.
    // If string is of the format "index=N1" then N2=N1

    if (aString == NULL)
    {
        return PVMFErrArgument;
    }

    // Go to end of "index="
    char* n1string = (char*)aString + 6;

    PV_atoi(n1string, 'd', oscl_strlen(n1string), aStartIndex);

    const char* n2string = oscl_strstr(aString, _STRLIT_CHAR("..."));

    if (n2string == NULL)
    {
        aEndIndex = aStartIndex;
    }
    else
    {
        // Go to end of "index=N1..."
        n2string += 3;

        PV_atoi(n2string, 'd', oscl_strlen(n2string), aEndIndex);
    }

    return PVMFSuccess;
}

PVMFStatus PVMFMP4FFParserNode::CreateNewArray(uint32** aTrackidList, uint32 aNumTracks)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, *aTrackidList = OSCL_ARRAY_NEW(uint32, aNumTracks););
    OSCL_FIRST_CATCH_ANY(leavecode, return PVMFErrNoMemory;);
    return PVMFSuccess;
}

uint32 PVMFMP4FFParserNode::GetNumAudioChannels(uint32 aId)
{
    uint32 num_channels = 0;
    uint8 audioObjectType;
    uint8 sampleRateIndex;
    uint32 samplesPerFrame;

    OSCL_HeapString<OsclMemAllocator> trackMIMEType;
    iMP4FileHandle->getTrackMIMEType(aId, trackMIMEType);

    if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR, oscl_strlen(PVMF_MIME_AMR)) == 0) ||
            (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0) ||
            (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0))
    {
        //always mono
        num_channels = 1;
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0)
    {
        int32 specinfosize =
            (int32)(iMP4FileHandle->getTrackDecoderSpecificInfoSize(aId));
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                iMP4FileHandle->getTrackDecoderSpecificInfoContent(aId);

            GetActualAacConfig(specinfoptr,
                               &audioObjectType,
                               &specinfosize,
                               &sampleRateIndex,
                               &num_channels,
                               &samplesPerFrame);
        }
    }

    return num_channels;
}

uint32 PVMFMP4FFParserNode::GetAudioSampleRate(uint32 aId)
{
    uint32 sample_rate = 0;
    uint32 num_channels;
    uint8 audioObjectType;
    uint8 sampleRateIndex;
    uint32 samplesPerFrame;

    const uint32 sample_freq_table[13] =
        {96000, 88200, 64000, 48000,
         44100, 32000, 24000, 22050,
         16000, 12000, 11025, 8000,
         7350
        };

    OSCL_HeapString<OsclMemAllocator> trackMIMEType;
    iMP4FileHandle->getTrackMIMEType(aId, trackMIMEType);

    if ((oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR, oscl_strlen(PVMF_MIME_AMR)) == 0) ||
            (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMR_IETF, oscl_strlen(PVMF_MIME_AMR_IETF)) == 0))
    {
        //always 8KHz
        sample_rate = 8000;
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_AMRWB_IETF, oscl_strlen(PVMF_MIME_AMRWB_IETF)) == 0)
    {
        //always 16KHz
        sample_rate = 16000;
    }
    else if (oscl_strncmp(trackMIMEType.get_str(), PVMF_MIME_MPEG4_AUDIO, oscl_strlen(PVMF_MIME_MPEG4_AUDIO)) == 0)
    {
        int32 specinfosize =
            (int32)(iMP4FileHandle->getTrackDecoderSpecificInfoSize(aId));
        if (specinfosize != 0)
        {
            // Retrieve the decoder specific info from file parser
            uint8* specinfoptr =
                iMP4FileHandle->getTrackDecoderSpecificInfoContent(aId);

            GetActualAacConfig(specinfoptr,
                               &audioObjectType,
                               &specinfosize,
                               &sampleRateIndex,
                               &num_channels,
                               &samplesPerFrame);
            if (sampleRateIndex < 13)
            {
                sample_rate = sample_freq_table[(uint32)sampleRateIndex];
            }
        }
    }
    return sample_rate;
}

uint32 PVMFMP4FFParserNode::GetAudioBitsPerSample(uint32 aId)
{
    OSCL_UNUSED_ARG(aId);
    //always 16 bits per samples
    return 16;
}

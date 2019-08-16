/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2014. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <cutils/properties.h>

#include "CFG_BT_File.h"
#include "CFG_BT_Default.h"
#include "CFG_file_lid.h"
#include "libnvram.h"
#include "bt_mtk.h"


/**************************************************************************
 *                  G L O B A L   V A R I A B L E S                       *
***************************************************************************/

BT_INIT_VAR_T btinit[1];
BT_INIT_CB_T  btinit_ctrl;

static uint16_t wOpCode;
static HCI_CMD_STATUS_T cmd_status;

static BOOL fgGetEFUSE = FALSE;
static UCHAR ucDefaultAddr[6];
static UCHAR ucZeroAddr[6];

/**************************************************************************
 *              F U N C T I O N   D E C L A R A T I O N S                 *
***************************************************************************/

static BOOL GORMcmd_HCC_Get_Local_BD_Addr(HC_BT_HDR *);
static VOID GORMevt_HCE_Get_Local_BD_Addr(VOID *);
static BOOL GORMcmd_HCC_Set_Local_BD_Addr(HC_BT_HDR *);
static BOOL GORMcmd_HCC_Set_PCM(HC_BT_HDR *);
static BOOL GORMcmd_HCC_Set_Radio(HC_BT_HDR *);
static BOOL GORMcmd_HCC_Set_TX_Power_Offset(HC_BT_HDR *);
static BOOL GORMcmd_HCC_Set_Sleep_Timeout(HC_BT_HDR *);
static BOOL GORMcmd_HCC_Coex_Performance_Adjust(HC_BT_HDR *);
static BOOL GORMcmd_HCC_Set_FW_SysLog(HC_BT_HDR *);
static BOOL GORMcmd_HCC_Set_SSP_Debug_Mode(HC_BT_HDR *);

static VOID GORMevt_HCE_Common_Complete(VOID *);
static BOOL WriteBDAddrToNvram(UCHAR *);

//===================================================================
// Combo chip
HCI_SEQ_T bt_init_preload_script_6630[] =
{
    {  GORMcmd_HCC_Get_Local_BD_Addr       }, /*0x1009*/
    {  GORMcmd_HCC_Set_Local_BD_Addr       }, /*0xFC1A*/
    {  GORMcmd_HCC_Set_PCM                 }, /*0xFC72*/
    {  GORMcmd_HCC_Set_Radio               }, /*0xFC79*/
    {  GORMcmd_HCC_Set_TX_Power_Offset     }, /*0xFC93*/
    {  GORMcmd_HCC_Set_Sleep_Timeout       }, /*0xFC7A*/
    {  GORMcmd_HCC_Coex_Performance_Adjust }, /*0xFC22*/
    {  GORMcmd_HCC_Set_FW_SysLog           }, /*0xFCBE*/
    {  GORMcmd_HCC_Set_SSP_Debug_Mode      }, /*0x1804*/
    {  0  },
};

/**************************************************************************
 *                          F U N C T I O N S                             *
***************************************************************************/

static bool is_memzero(unsigned char *buf, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        if (*(buf+i) != 0) return FALSE;
    }
    return TRUE;
}

static int bt_read_nvram(unsigned char *pucNvRamData)
{
    F_ID bt_nvram_fd;
    int rec_size = 0;
    int rec_num = 0;
    ap_nvram_btradio_struct bt_nvram;

    int nvram_ready_retry = 0;
    char nvram_init_val[PROPERTY_VALUE_MAX];

    LOG_TRC();

    /* Sync with Nvram daemon ready */
    do {
        if (property_get("service.nvram_init", nvram_init_val, NULL) &&
            0 == strcmp(nvram_init_val, "Ready"))
            break;
        else {
            nvram_ready_retry ++;
            usleep(500000);
        }
    } while(nvram_ready_retry < 10);

    LOG_DBG("Get NVRAM ready retry %d\n", nvram_ready_retry);
    if (nvram_ready_retry >= 10) {
        LOG_ERR("Get NVRAM restore ready fails!\n");
        return -1;
    }

    bt_nvram_fd = NVM_GetFileDesc(AP_CFG_RDEB_FILE_BT_ADDR_LID, &rec_size, &rec_num, ISREAD);
    if (bt_nvram_fd.iFileDesc < 0) {
        LOG_WAN("Open BT NVRAM fails errno %d\n", errno);
        return -1;
    }

    if (rec_num != 1) {
        LOG_ERR("Unexpected record num %d", rec_num);
        NVM_CloseFileDesc(bt_nvram_fd);
        return -1;
    }

    if (rec_size != sizeof(ap_nvram_btradio_struct)) {
        LOG_ERR("Unexpected record size %d ap_nvram_btradio_struct %zu",
                rec_size, sizeof(ap_nvram_btradio_struct));
        NVM_CloseFileDesc(bt_nvram_fd);
        return -1;
    }

    if (read(bt_nvram_fd.iFileDesc, &bt_nvram, rec_num*rec_size) < 0) {
        LOG_ERR("Read NVRAM fails errno %d\n", errno);
        NVM_CloseFileDesc(bt_nvram_fd);
        return -1;
    }

    NVM_CloseFileDesc(bt_nvram_fd);
    memcpy(pucNvRamData, &bt_nvram, sizeof(ap_nvram_btradio_struct));

    return 0;
}

void notify_thread_exit(void)
{
    pthread_mutex_lock(&btinit_ctrl.mutex);
    cmd_status = CMD_TERMINATE;
    /* Wake up command tx thread */
    pthread_cond_signal(&btinit_ctrl.cond);
    pthread_mutex_unlock(&btinit_ctrl.mutex);
}

VOID *GORM_FW_Init_Thread(VOID *ptr)
{
    if (ptr != NULL) {
        LOG_DBG("Useless BT log\n");
    }
    INT32 i = 0;
    HC_BT_HDR  *p_buf = NULL;
    bt_vendor_op_result_t ret = BT_VND_OP_RESULT_FAIL;

    LOG_DBG("FW init thread starts\n");

    pthread_mutexattr_init(&btinit_ctrl.attr);
    pthread_mutexattr_settype(&btinit_ctrl.attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&btinit_ctrl.mutex, &btinit_ctrl.attr);
    pthread_cond_init(&btinit_ctrl.cond, NULL);
    btinit->cur_script = bt_init_preload_script_6630;
    memcpy(ucDefaultAddr, stBtDefault_6630.addr, 6);

    /* Can not find matching script, simply skip */
    if ((btinit->cur_script) == NULL) {
        LOG_ERR("No matching init script\n");
        goto exit;
    }

    if ((0 == memcmp(btinit->bt_nvram.fields.addr, ucDefaultAddr, 6)) ||
        (0 == memcmp(btinit->bt_nvram.fields.addr, ucZeroAddr, 6))) {
        /* NVRAM BD address default value */
        /* Want to retrieve module eFUSE address on combo chip */
        fgGetEFUSE = TRUE;
    }

    if (fgGetEFUSE) /* perform GORMcmd_HCC_Get_Local_BD_Addr */
        i = 0;
    else /* skip GORMcmd_HCC_Get_Local_BD_Addr */
        i = 1;

    while (btinit->cur_script[i].command_func && cmd_status != CMD_TERMINATE) {
        p_buf = NULL;

        p_buf = (HC_BT_HDR *) alloc_buffer(BT_HC_HDR_SIZE + HCI_CMD_MAX_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;

            pthread_mutex_lock(&btinit_ctrl.mutex);
            if (cmd_status != CMD_TERMINATE) {
                cmd_status = CMD_PENDING;
                pthread_mutex_unlock(&btinit_ctrl.mutex);
            } else {
                pthread_mutex_unlock(&btinit_ctrl.mutex);
                break;
            }

            if (btinit->cur_script[i].command_func(p_buf) == FALSE) {
                LOG_ERR("Send command %d fails\n", i);
                free_buffer(p_buf);
                goto exit;
            }
        }
        else {
            LOG_ERR("Alloc command %d buffer fails\n", i);
            goto exit;
        }

        /* Wait for event returned */
        pthread_mutex_lock(&btinit_ctrl.mutex);
        while (cmd_status == CMD_PENDING) {
            pthread_cond_wait(&btinit_ctrl.cond, &btinit_ctrl.mutex);
        }

        if (cmd_status == CMD_FAIL) {
            LOG_ERR("The event of command %d error\n", i);
            pthread_mutex_unlock(&btinit_ctrl.mutex);
            goto exit;
        }
        else {
            LOG_DBG("The event of command %d success\n", i);
            pthread_mutex_unlock(&btinit_ctrl.mutex);
        }

        i ++;
    }

    ret = BT_VND_OP_RESULT_SUCCESS;

exit:
    pthread_mutexattr_destroy(&btinit_ctrl.attr);
    pthread_mutex_destroy(&btinit_ctrl.mutex);
    pthread_cond_destroy(&btinit_ctrl.cond);

    callFirmwareConfigured(ret);

    btinit_ctrl.worker_thread_running = FALSE;
    return NULL;
}

bool BT_InitDevice(PUCHAR pucNvRamData) 
{
    LOG_DBG("BT_InitDevice\n");

    memset(btinit, 0, sizeof(BT_INIT_VAR_T));
    btinit_ctrl.worker_thread_running = FALSE;

    btinit->chip_id = 0x6630;
    /* Copy configuration data */
    memcpy(btinit->bt_nvram.raw, pucNvRamData, sizeof(ap_nvram_btradio_struct));

    if (pthread_create(&btinit_ctrl.worker_thread, NULL, \
          GORM_FW_Init_Thread, NULL) != 0) {
        LOG_ERR("Create FW init thread fails\n");
        return FALSE;
    }
    else {
        btinit_ctrl.worker_thread_running = TRUE;
        return TRUE;
    }
}

int mtk_fw_cfg(void)
{
    unsigned char ucNvRamData[sizeof(ap_nvram_btradio_struct)] = {0};

    /* Read NVRAM data */
    if ((bt_read_nvram(ucNvRamData) < 0) ||
          is_memzero(ucNvRamData, sizeof(ap_nvram_btradio_struct))) {
        LOG_WAN("Read NVRAM data fails or NVRAM data all zero!!\n");
        LOG_WAN("Use mt6630 default value\n");
        /* Use MT6630 default value */
        memcpy(ucNvRamData, &stBtDefault_6630, sizeof(ap_nvram_btradio_struct));
    }

    return (BT_InitDevice(ucNvRamData) == true ? 0 : -1);
}

void BT_Cleanup(void)
{
    /* Cancel any remaining running thread */
    if (btinit_ctrl.worker_thread_running)
        notify_thread_exit();

    /*
     * Since Android M, pthread_exit only frees mapped space (including pthread_internal_t and thread stack)
     * for detached thread; for joinable thread, it is left for the pthread_join caller to clean up.
     *
     * The thread type is specified when thread create, default joinable for new thread if not set attribute with
     * PTHREAD_CREATE_DETATCHED, or not call pthread_detach before thread exit.
     */
    /* Always do pthread_join no matter the target thread has exited or not */
    pthread_join(btinit_ctrl.worker_thread, NULL);

    return;
}

static BOOL GORMcmd_HCC_Get_Local_BD_Addr(HC_BT_HDR *p_cmd)
{
    uint8_t *p, ret;
    wOpCode = 0x1009;

    p_cmd->len = 3;
    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    *p++ = 0;

    LOG_DBG("GORMcmd_HCC_Get_Local_BD_Addr\n");
    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Get_Local_BD_Addr);
    return ret;
}

static VOID GORMevt_HCE_Get_Local_BD_Addr(VOID *p_evt)
{
    HC_BT_HDR *p_buf = (HC_BT_HDR *)p_evt;
    uint8_t *p;
    uint8_t event, status;
    uint16_t opcode;
    BOOL success;

    UCHAR ucBDAddr[6];

    LOG_DBG("GORMevt_HCE_Get_Local_BD_Addr\n");

    p = (uint8_t *)(p_buf + 1);
    event = *p;
    p += 3;
    STREAM_TO_UINT16(opcode, p)
    status = *p++;

    if ((event == 0x0E) && /* Command Complete Event */
        (opcode == 0x1009) && /* OpCode correct */
        (status == 0)) /* Success */
    {
        success = TRUE;
    }
    else {
        success = FALSE;
    }

    ucBDAddr[5] = *p++;
    ucBDAddr[4] = *p++;
    ucBDAddr[3] = *p++;
    ucBDAddr[2] = *p++;
    ucBDAddr[1] = *p++;
    ucBDAddr[0] = *p++;

    LOG_WAN("Retrieve eFUSE address: %02x-%02x-%02x-%02x-%02x-%02x\n",
            ucBDAddr[0], ucBDAddr[1], ucBDAddr[2], ucBDAddr[3], ucBDAddr[4], ucBDAddr[5]);

    memcpy(btinit->bt_nvram.fields.addr, ucBDAddr, 6);

    free_buffer(p_buf);

    pthread_mutex_lock(&btinit_ctrl.mutex);
    cmd_status = success ? CMD_SUCCESS : CMD_FAIL;
    /* Wake up command tx thread */
    pthread_cond_signal(&btinit_ctrl.cond);
    pthread_mutex_unlock(&btinit_ctrl.mutex);

    return;
}

static BOOL GORMcmd_HCC_Set_Local_BD_Addr(HC_BT_HDR *p_cmd)
{
    uint8_t *p, ret;
    wOpCode = 0xFC1A;

    if ((0 == memcmp(btinit->bt_nvram.fields.addr, ucDefaultAddr, 6)) ||
        (0 == memcmp(btinit->bt_nvram.fields.addr, ucZeroAddr, 6))) {
        if (fgGetEFUSE) {
            LOG_WAN("eFUSE address default value\n");
        }
        else {
            LOG_WAN("NVRAM BD address default value\n");
        }

        /* Save BD address to NVRAM */
        WriteBDAddrToNvram(btinit->bt_nvram.fields.addr);
    }
    else {
        if (fgGetEFUSE) {
            LOG_WAN("eFUSE address has valid value\n");
            /* Save BD address to NVRAM */
            WriteBDAddrToNvram(btinit->bt_nvram.fields.addr);
         }
         else {
            LOG_WAN("NVRAM BD address has valid value\n");
         }
    }

    fgGetEFUSE = FALSE; /* Clear flag */

    p_cmd->len = 9;
    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    *p++ = 6;

    /* HCI cmd params */
    *p++ = btinit->bt_nvram.fields.addr[5];
    *p++ = btinit->bt_nvram.fields.addr[4];
    *p++ = btinit->bt_nvram.fields.addr[3];
    *p++ = btinit->bt_nvram.fields.addr[2];
    *p++ = btinit->bt_nvram.fields.addr[1];
    *p++ = btinit->bt_nvram.fields.addr[0];

    LOG_DBG("GORMcmd_HCC_Set_Local_BD_Addr\n");

    LOG_WAN("Write BD address: %02x-%02x-%02x-%02x-%02x-%02x\n",
            btinit->bt_nvram.fields.addr[0], btinit->bt_nvram.fields.addr[1],
            btinit->bt_nvram.fields.addr[2], btinit->bt_nvram.fields.addr[3],
            btinit->bt_nvram.fields.addr[4], btinit->bt_nvram.fields.addr[5]);

    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

static BOOL GORMcmd_HCC_Set_PCM(HC_BT_HDR *p_cmd)
{
    uint8_t *p, ret;
    wOpCode = 0xFC72;

    p_cmd->len = 7;
    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    *p++ = 4;

    /* HCI cmd params */
    *p++ = btinit->bt_nvram.fields.Codec[0];
    *p++ = btinit->bt_nvram.fields.Codec[1];
    *p++ = btinit->bt_nvram.fields.Codec[2];
    *p++ = btinit->bt_nvram.fields.Codec[3];

    LOG_DBG("GORMcmd_HCC_Set_PCM\n");

    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

static BOOL GORMcmd_HCC_Set_Radio(HC_BT_HDR *p_cmd)
{
    uint8_t *p, ret;
    wOpCode = 0xFC79;

    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    p_cmd->len = 9;
    *p++ = 6;

    /* HCI cmd params */
    *p++ = btinit->bt_nvram.fields.Radio[0];
    *p++ = btinit->bt_nvram.fields.Radio[1];
    *p++ = btinit->bt_nvram.fields.Radio[2];
    *p++ = btinit->bt_nvram.fields.Radio[3];
    *p++ = btinit->bt_nvram.fields.Radio[4];
    *p++ = btinit->bt_nvram.fields.Radio[5];
    LOG_DBG("GORMcmd_HCC_Set_Radio\n");

    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

static BOOL GORMcmd_HCC_Set_TX_Power_Offset(HC_BT_HDR *p_cmd)
{
    uint8_t *p, ret;
    wOpCode = 0xFC93;

    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    p_cmd->len = 6;
    *p++ = 3;

    /* HCI cmd params */
    *p++ = btinit->bt_nvram.fields.TxPWOffset[0];
    *p++ = btinit->bt_nvram.fields.TxPWOffset[1];
    *p++ = btinit->bt_nvram.fields.TxPWOffset[2];

    LOG_DBG("GORMcmd_HCC_Set_TX_Power_Offset\n");
    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

static BOOL GORMcmd_HCC_Set_Sleep_Timeout(HC_BT_HDR *p_cmd)
{
    uint8_t *p, ret;
    wOpCode = 0xFC7A;

    p_cmd->len = 10;
    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    *p++ = 7;

    /* HCI cmd params */
    *p++ = btinit->bt_nvram.fields.Sleep[0];
    *p++ = btinit->bt_nvram.fields.Sleep[1];
    *p++ = btinit->bt_nvram.fields.Sleep[2];
    *p++ = btinit->bt_nvram.fields.Sleep[3];
    *p++ = btinit->bt_nvram.fields.Sleep[4];
    *p++ = btinit->bt_nvram.fields.Sleep[5];
    *p++ = btinit->bt_nvram.fields.Sleep[6];

    LOG_DBG("GORMcmd_HCC_Set_Sleep_Timeout\n");
    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

static BOOL GORMcmd_HCC_Coex_Performance_Adjust(HC_BT_HDR *p_cmd)
{
    uint8_t *p, ret;
    wOpCode = 0xFC22;

    p_cmd->len = 9;
    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    *p++ = 6;

    /* HCI cmd params */
    *p++ = btinit->bt_nvram.fields.CoexAdjust[0];
    *p++ = btinit->bt_nvram.fields.CoexAdjust[1];
    *p++ = btinit->bt_nvram.fields.CoexAdjust[2];
    *p++ = btinit->bt_nvram.fields.CoexAdjust[3];
    *p++ = btinit->bt_nvram.fields.CoexAdjust[4];
    *p++ = btinit->bt_nvram.fields.CoexAdjust[5];

    LOG_DBG("GORMcmd_HCC_Coex_Performance_Adjust\n");
    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

static BOOL GORMcmd_HCC_Set_FW_SysLog(HC_BT_HDR *p_cmd)
{
    char bt_syslog_val[PROPERTY_VALUE_MAX];
    uint8_t *p, ret;
    wOpCode = 0xFCBE;

    p_cmd->len = 4;
    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    *p++ = 1;

    /* HCI cmd params */
    if (property_get("persist.bt.syslog.enable", bt_syslog_val, NULL) &&
        0 == strcmp(bt_syslog_val, "1")) {
        *p++ = 0x05;
        LOG_DBG("GORMcmd_HCC_Set_FW_SysLog enable\n");
    }
    else {
        *p++ = 0x00;
        LOG_DBG("GORMcmd_HCC_Set_FW_SysLog disable\n");
    }
    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

/*
 * Simple Pairing Debug Mode:
 *
 * \brief
 *  Send HCI_Write_Simple_Pairing_Debug_Mode with 'enabled' parameter if Bluetooth SSP Debug Mode
 *  is set enabled in Engineer Mode UI for debugging purpose.
 *  This command configures the BR/EDR Controller to use a predefined Diffie Hellman private key
 *  for Simple Pairing, in order for debug equipment to be able to determine the link key
 *  and therefore, be able to monitor the encrypted connection.
 *
 *  Command parameters:
 *  - 0x00: Simple Pairing debug mode disabled (default)
 *  - 0x01: Simple Pairing debug mode enabled
 *
 *  Event generated:
 *  - When HCI_Write_Simple_Pairing_Debug_Mode command has completed, a Command Complete event
 *    shall be generated.
 */
static BOOL GORMcmd_HCC_Set_SSP_Debug_Mode(HC_BT_HDR *p_cmd)
{
    char bt_ssp_debug_val[PROPERTY_VALUE_MAX];
    uint8_t *p, ret;
    wOpCode = 0x1804;

    p_cmd->len = 4;
    p = (uint8_t *)(p_cmd + 1);
    UINT16_TO_STREAM(p, wOpCode);
    *p++ = 1;

    /* HCI cmd params */
    if (property_get("persist.bt.sspdebug.enable", bt_ssp_debug_val, NULL) &&
        0 == strcmp(bt_ssp_debug_val, "1")) {
        *p++ = 0x01;
        LOG_DBG("GORMcmd_HCC_Set_SSP_Debug_Mode enable\n");
    }
    else {
        *p++ = 0x00;
        LOG_DBG("GORMcmd_HCC_Set_SSP_Debug_Mode disable\n");
    }
    ret = transmit_old(wOpCode, p_cmd, GORMevt_HCE_Common_Complete);
    return ret;
}

static VOID GORMevt_HCE_Common_Complete(VOID *p_evt)
{
    HC_BT_HDR *p_buf = (HC_BT_HDR *)p_evt;
    uint8_t *p;
    uint8_t event, status;
    uint16_t opcode;
    BOOL success;

    LOG_DBG("GORMevt_HCE_Common_Complete\n");

    p = (uint8_t *)(p_buf + 1);
    event = *p;
    p += 3;
    STREAM_TO_UINT16(opcode, p);
    status = *p;

    if ((event == 0x0E) && /* Command Complete Event */
        (opcode == wOpCode) && /* OpCode correct */
        (status == 0)) /* Success */
    {
        success = TRUE;
    }
    else {
        success = FALSE;
    }

    free_buffer(p_buf);

    pthread_mutex_lock(&btinit_ctrl.mutex);
    cmd_status = success ? CMD_SUCCESS : CMD_FAIL;
    /* Wake up command tx thread */
    pthread_cond_signal(&btinit_ctrl.cond);
    pthread_mutex_unlock(&btinit_ctrl.mutex);

    return;
}

static BOOL WriteBDAddrToNvram(UCHAR *pucBDAddr)
{
    INT32 bt_fd = 0;

    bt_fd = open("/protect_s/properties/BT_Addr", O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, 0666);

    /* Update BD address */
    if (write(bt_fd, pucBDAddr, 6) < 0) {
        LOG_ERR("Write BT NVRAM fails errno %d\n", errno);
        close(bt_fd);
        return FALSE;
    }

    close(bt_fd);
    return TRUE;
}

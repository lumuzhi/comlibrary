/**
 * @file   示例 can 收发数据
 * @details
 * @author
 * @date
 * @version
**/

/* include */
#include "app_can.h"
#include "app_can1.h"
#include "app_log.h"
#include "app_qzq.h"
#include "app_paras.h"
#include "app_update.h"
#include "head_protocol.h"
#include "app_qzq_can.h"
#include "app_new_qzq.h"

/* macro */
#define CAN1_DEV_NAME             "can1"      /* CAN 设备名称 */
#define CAN1_RECV_BUFF_SIZE       1024
#define CAN1_RECV_RB_BUFF_SIZE    (CAN1_RECV_BUFF_SIZE * 5)
#define CAN1_SEND_BUFF_SIZE       1024
#define CAN1_SEND_RB_BUFF_SIZE    (CAN1_SEND_BUFF_SIZE * 10)
#define APP_CAN1_TIME             3000  //3秒没有收到CAN数据，重新初始化，5秒LED灯反应状态

/* type declaration */
typedef struct
{
    struct rt_can_msg    rxmsg;
    rt_sem_t             recv_sem;
    uint8_t              *recv_buff;   //CAN1接收到数据组包buff
    struct rt_ringbuffer *recv_rb;     //CAN1接收环形缓冲区
    uint8_t              *read_buff;   //CAN1读取接收环形缓冲区的数据进行数据处理的buff
    rt_sem_t             deal_sem;
    uint8_t              *send_buff;   //CAN1发送时从环形缓冲区获取的数据
    struct rt_ringbuffer *send_rb;     //CAN1发送环形缓冲区
}app_can1_local_t;

/* variable declaration */
app_can1_env_t app_can1_env;
#define env app_can1_env
app_can1_local_t app_can1_local;
#define local app_can1_local

/* function declaration */
/**
 * @brief
 * @param
 * @return
 * @note
**/
int app_can1_send(uint8_t board_id, uint8_t *data, uint16_t size)
{
    can_id_u can_id;
    struct rt_can_msg can_msg;
    uint16_t whole_count, remain_count, send_cnts, can_lens;
    bool frame_state;
    static uint8_t send_count = 0;

    if((data == NULL) || (size == 0) || (env.dev_state != DEV_INIT_SUCCESS)) {
        return -1;
    }
    can_id.word = 0;
    memset(&can_msg, 0, sizeof(struct rt_can_msg));
    can_id.bits.source = CLB_BOARD;
    can_id.bits.target = board_id;
    whole_count = size / CAN_MAX_DLEN;
    remain_count = size % CAN_MAX_DLEN;
    if((whole_count > 1) || ((whole_count == 1) && (remain_count > 0)))
    {
        frame_state = true;
        send_cnts = whole_count;
        if(remain_count > 0) {
            send_cnts++;
        }
    }
    else
    {
        frame_state = false;
        send_cnts = 1;
    }
    for(uint16_t i = 0, index = 0; i < send_cnts; i++)
    {
        if(frame_state)
        {
            if(i == 0)
            {
                can_id.bits.type = BEGIN_FRAME;
                can_lens = CAN_MAX_DLEN;
            }
            else if(i == (send_cnts - 1))
            {
                can_id.bits.type = FINISH_FRAME;
                if (remain_count > 0) {
                    can_lens = remain_count;
                }
                else {
                    can_lens = CAN_MAX_DLEN;
                }
            }
            else
            {
                can_id.bits.type = MIDDLE_FRAME;
                can_lens = CAN_MAX_DLEN;
            }
        }
        else
        {
            can_id.bits.type = SINGLE_FRAME;
            if(whole_count == 1) {
                can_lens = CAN_MAX_DLEN;
            }
            else {
                can_lens = remain_count;
            }
        }
        send_count++;
        can_id.bits.count = send_count;
        can_msg.id = can_id.word;
        can_msg.ide = RT_CAN_EXTID;
        can_msg.rtr = RT_CAN_DTR;
        can_msg.len = can_lens;
        memset(can_msg.data, 0, CAN_MAX_DLEN);
        rt_memcpy(can_msg.data, &data[index], can_lens);
        index += can_lens;
        rt_device_write(env.dev_handle, 0, &can_msg, sizeof(struct rt_can_msg));
    }
    return size;
}

/**
 * @brief
 * @param
 * @return
 * @note   CAN1接收数据中断回调函数
**/
static rt_err_t app_can1_recv_cb(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(local.recv_sem);
    return RT_EOK;
}

/**
 * @brief
 * @param
 * @return
 * @note
**/
static void app_can1_recv_thread_entry(void * paras)
{
    can_id_u can_rx_id;
    uint8_t send_count;
    uint16_t recv_size;
    while(1)
    {
        env.life_cnt++;
        local.rxmsg.hdr = -1;
        if (rt_sem_trytake(local.recv_sem) == RT_EOK)
        {
            rt_device_read(env.dev_handle, 0, &local.rxmsg, sizeof(struct rt_can_msg));
            can_rx_id.word = local.rxmsg.id;
            if((app_qzq_can_env.com_state) && (can_rx_id.bits.source == app_qzq_can_env.cur_updata_qzq)) {
                goto qzq_can;
            }

//            can_rx_id.word = local.rxmsg.id;
            if((false == app_qzq_can_env.com_state) && (can_rx_id.bits.target == CLB_BOARD))
            {
                app_can_env.can_size = local.rxmsg.len;
                switch(can_rx_id.bits.type)
                {
                case SINGLE_FRAME:
                    recv_size = local.rxmsg.len;
                    if(recv_size <= (rt_ringbuffer_space_len(local.recv_rb) + 2))
                    {
                        if(rt_ringbuffer_put(local.recv_rb, (uint8_t *)&recv_size, 2) == 2)
                        {
                            if(rt_ringbuffer_put(local.recv_rb, local.rxmsg.data, recv_size) != recv_size) {
                                rt_ringbuffer_reset(local.recv_rb);
                            }
                            recv_size = 0;
                            rt_sem_release(local.deal_sem);
                        }
                        else
                        {
                            rt_ringbuffer_reset(local.recv_rb);
                            recv_size = 0;
                        }
                    }
                    send_count = 0;
                    break;
                case BEGIN_FRAME:
                    recv_size = local.rxmsg.len;
                    send_count = can_rx_id.bits.count;
                    memset(local.recv_buff, 0, CAN1_RECV_BUFF_SIZE);
                    memcpy(local.recv_buff, local.rxmsg.data, local.rxmsg.len);
                    send_count++;
                    break;
                case MIDDLE_FRAME:
                    if((send_count == can_rx_id.bits.count) && ((recv_size + local.rxmsg.len) <= CAN1_RECV_BUFF_SIZE))
                    {
                        memcpy(&local.recv_buff[recv_size], local.rxmsg.data, local.rxmsg.len);
                        recv_size += local.rxmsg.len;
                        send_count++;
                    }
                    break;
                case FINISH_FRAME:
                    if((send_count == can_rx_id.bits.count) && ((recv_size + local.rxmsg.len) <= CAN1_RECV_BUFF_SIZE))
                    {
                        memcpy(&local.recv_buff[recv_size], local.rxmsg.data, local.rxmsg.len);
                        recv_size += local.rxmsg.len;
                        if(recv_size <= (rt_ringbuffer_space_len(local.recv_rb) + 2))
                        {
                            if(rt_ringbuffer_put(local.recv_rb, (uint8_t *)&recv_size, 2) == 2)
                            {
                                if(rt_ringbuffer_put(local.recv_rb, local.recv_buff, recv_size) != recv_size) {
                                    rt_ringbuffer_reset(local.recv_rb);
                                }
                                recv_size = 0;
                                rt_sem_release(local.deal_sem);
                            }
                            else
                            {
                                rt_ringbuffer_reset(local.recv_rb);
                                recv_size = 0;
                            }
                        }
                        send_count = 0;
                    }
                    break;
                default:
                    break;
                }
            }
            else
            {
                qzq_can:
                    app_qzq_can1_recv(local.rxmsg);
            }
        }
        else {
            rt_thread_delay(1);
        }
    }
}

/**
 * @brief
 * @param
 * @return
 * @note
**/
static void app_can1_deal_thread_entry(void * paras)
{
    uint16_t recv_size;
    while(1)
    {
        if(rt_sem_take(local.deal_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            if(rt_ringbuffer_data_len(local.recv_rb))
            {
                if(rt_ringbuffer_get(local.recv_rb, (uint8_t *)&recv_size, 2) == 2)
                {
                    if(rt_ringbuffer_get(local.recv_rb, local.read_buff, recv_size) == recv_size)
                    {
                        app_qzq_data_deal(local.read_buff, recv_size);
                        app_paras_qzq_deal(local.read_buff, recv_size);
                        app_update_data_deal(local.read_buff, recv_size);
                    }
                    else {
                        rt_ringbuffer_reset(local.recv_rb);
                    }
                }
                else {
                    rt_ringbuffer_reset(local.recv_rb);
                }
            }
        }
    }
}

/**
 * @brief
 * @param
 * @return
 * @note
**/
void app_can1_thread(void)
{
    if(env.dev_state == DEV_INIT_SUCCESS)
    {
        env.recv_thread = rt_thread_create("can1_recv", app_can1_recv_thread_entry, \
                RT_NULL, APP_CAN1_RECV_STACK, APP_CAN1_RECV_THREAD_PRO, 20);
        if(env.recv_thread != RT_NULL) {
            rt_thread_startup(env.recv_thread);
        }
        else {
            app_log_msg(LOG_LVL_ERROR, true, "can1 recv thread create failed");
        }
        env.deal_thread = rt_thread_create("can1_deal", app_can1_deal_thread_entry, \
                RT_NULL, APP_CAN1_DEAL_STACK, APP_CAN1_DEAL_THREAD_PRO, 20);
        if(env.deal_thread != RT_NULL) {
            rt_thread_startup(env.deal_thread);
        }
        else {
            app_log_msg(LOG_LVL_ERROR, true, "can1 deal thread create failed");
        }
    }
}

/**
 * @brief
 * @param
 * @return
 * @note
**/
void app_can1_init(void)
{
    env.dev_state = DEV_INIT_NULL;
    env.life_cnt = 0;
    local.recv_buff = rt_malloc(CAN1_RECV_BUFF_SIZE);
    if(local.recv_buff == NULL)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "app can1 malloc recv buff error");
        return ;
    }
    local.read_buff = rt_malloc(CAN1_RECV_BUFF_SIZE);
    if(local.read_buff == NULL)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "app can1 malloc read buff error");
        return ;
    }
    local.recv_rb = rt_ringbuffer_create(CAN1_RECV_RB_BUFF_SIZE);
    if(local.recv_rb == NULL)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "app can1 recv rb create error");
        return ;
    }
    local.send_buff = rt_malloc(CAN1_SEND_BUFF_SIZE);
    if(local.send_buff == NULL)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "app can1 malloc send buff error");
        return ;
    }
    local.send_rb = rt_ringbuffer_create(CAN1_SEND_RB_BUFF_SIZE);
    if(local.send_rb == NULL)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "app can1 send rb create error");
        return ;
    }
    env.dev_handle = rt_device_find(CAN1_DEV_NAME);
    if(!env.dev_handle)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "can1 not find device");
        return ;
    }
    if(rt_device_open(env.dev_handle, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX) != RT_EOK)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "can1 device set interrupt failed");
        return ;
    }
    if(rt_device_control(env.dev_handle, RT_CAN_CMD_SET_BAUD, (void*) CAN500kBaud) != RT_EOK)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "can1 device set baud failed");
        return ;
    }
    if(rt_device_control(env.dev_handle, RT_CAN_CMD_SET_MODE, (void*) RT_CAN_MODE_NORMAL) != RT_EOK)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "can1 device set mode failed");
        return ;
    }
    local.recv_sem = rt_sem_create("can1_recv_sem", 0, RT_IPC_FLAG_PRIO);
    if(local.recv_sem == RT_NULL)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "can1 device set recv sem failed");
        return ;
    }
    local.deal_sem = rt_sem_create("can1_deal_sem", 0, RT_IPC_FLAG_PRIO);
    if(local.deal_sem == RT_NULL)
    {
        env.dev_state = DEV_INIT_FAILED;
        app_log_msg(LOG_LVL_ERROR, true, "can1 device set deal sem failed");
        return ;
    }
    rt_device_set_rx_indicate(env.dev_handle, app_can1_recv_cb);
    env.dev_state = DEV_INIT_SUCCESS;
    app_log_msg(LOG_LVL_INFO, true, "app can1 init success");
}
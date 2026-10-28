/**
 * @file boot.c
 * @brief microFlow RS485 在线升级入口，兼容 ST_588 旧上位机。
 *
 * 上电等待 abc -> 菜单 1 -> YMODEM 接收 -> 复位 -> 检查向量并启动 APP。
 * 只覆盖 APP，末页标定/参数保持不动；中断升级后通过重新上电再次命中。
 * 提示文本、拼写及 CR/LF 是旧上位机协议的一部分，修改前需联调验证。
 * 不保存升级完成标记，image_valid 仅检查入口，不能判断固件是否完整。
 */
#include "boot.h"
#include <string.h>

enum { SOH=1, STX=2, EOT=4, ACK=6, NAK=0x15, YM_CANCEL=0x18, CRC_REQUEST='C' };
/* SOH/STX 单独读取；这里依次保存序号、反码、最多 1024 字节数据及 CRC。 */
static uint8_t packet[1028];
static UART_HandleTypeDef *port;
static char file_name[65];
static uint32_t image_size;
enum { RECEIVE_ERROR, RECEIVE_LIMIT, RECEIVE_DATA, RECEIVE_ABORT };
static int receive_result;

static void send_byte(uint8_t v) { HAL_UART_Transmit(port, &v, 1, 1000); }
static void message(const char *s) {
    HAL_UART_Transmit(port, (uint8_t *)s, (uint16_t)strlen(s), 5000);
}
/* CRC16/XMODEM：多项式 0x1021，初值 0，无反转；线上先发送 CRC 高字节。 */
static uint16_t crc16(const uint8_t *data, uint32_t length) {
    uint16_t crc=0;
    while (length--) {
        crc ^= (uint16_t)*data++ << 8;
        for (unsigned bit=0; bit<8; ++bit)
            crc=(uint16_t)((crc<<1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    }
    return crc;
}
/* 返回数据长度 128/1024；0=EOT；-1=错误/超时；-2=双 CAN；-3=A/a。
 * 9600 波特率下 1 KB 包需超过 1 秒，完整包接收预留 5 秒。
 * A/a 仅在等待包起始字节时作为取消，不检查包内二进制数据。
 */
static int receive_packet(void) {
    uint8_t start;
    if (HAL_UART_Receive(port, &start, 1, 2000) != HAL_OK) return -1;
    if (start == EOT) return 0;
    if (start == 'a' || start == 'A') return -3;
    if (start == YM_CANCEL) {
        uint8_t next;
        return HAL_UART_Receive(port, &next, 1, 1000) == HAL_OK && next == YM_CANCEL ? -2 : -1;
    }
    int length=start == SOH ? 128 : start == STX ? 1024 : -1;
    if (length<0) return -1;
    if (HAL_UART_Receive(port, packet, (uint16_t)(length+4), 5000) != HAL_OK) return -1;
    if ((uint8_t)(packet[0]^packet[1]) != 0xFFU) return -1;
    uint16_t expected=(uint16_t)((packet[length+2]<<8)|packet[length+3]);
    return crc16(packet+2, (uint32_t)length) == expected ? length : -1;
}
/* 当前 APP 栈位于 SRAM1，允许栈顶恰为 0x2000C000。
 * Reset_Handler 必须为 Thumb 地址且落在 APP 区；不代替整包校验。
 */
static int image_valid(void) {
    uint32_t stack=*(volatile uint32_t *)BOOT_APP_ADDRESS;
    uint32_t entry=*(volatile uint32_t *)(BOOT_APP_ADDRESS+4);
    return stack>0x20000000UL && stack<=0x2000C000UL && (stack & 7U)==0 &&
           (entry & 1U) && (entry & ~1UL)>=BOOT_APP_ADDRESS && (entry & ~1UL)<BOOT_APP_END;
}
/* 固定擦除第 16～62 页。第 0～15 页为 Boot，第 63 页为参数。 */
static int erase_app(void) {
    FLASH_EraseInitTypeDef erase={0};
    uint32_t error;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.Banks=FLASH_BANK_1;
    erase.Page=(BOOT_APP_ADDRESS-FLASH_BASE)/FLASH_PAGE_SIZE;
    erase.NbPages=BOOT_APP_SIZE/FLASH_PAGE_SIZE;
    if (HAL_FLASH_Unlock() != HAL_OK) return 0;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);
    HAL_StatusTypeDef status=HAL_FLASHEx_Erase(&erase, &error);
    HAL_FLASH_Lock();
    return status == HAL_OK;
}
/* L432 每次编程 8 字节，使用 memcpy 避免串口包缓冲的非对齐读取。
 * size 为文件剩余有效字节；末尾不足 8 字节补 FF，不写 YMODEM 的填充。
 * 使用减法检查边界，避免 address+size 溢出；每个双字写后回读。
 */
static int write_app(uint32_t address, const uint8_t *data, uint32_t size) {
    if ((address & 7U) || address<BOOT_APP_ADDRESS || address>=BOOT_APP_END ||
        size>BOOT_APP_END-address) return 0;
    uint32_t padded=(size+7U)&~7U;
    if (padded>BOOT_APP_END-address || HAL_FLASH_Unlock() != HAL_OK) return 0;
    for (uint32_t offset=0; offset<padded; offset+=8) {
        uint64_t value=UINT64_MAX;
        uint32_t count=size-offset<8 ? size-offset : 8;
        memcpy(&value, data+offset, count);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address+offset, value) != HAL_OK ||
            *(volatile uint64_t *)(address+offset) != value) {
            HAL_FLASH_Lock();
            return 0;
        }
    }
    HAL_FLASH_Lock();
    return 1;
}
/* 第 0 包：文件名 + NUL + 十进制长度 + NUL/空格。
 * 解析必须限定在本包内，长度在擦除前检查，文件名只用于原版成功提示。
 */
static int header_size(uint32_t length, uint32_t *size) {
    const uint8_t *data=packet+2;
    uint32_t index=0, value=0;
    while (index<length && data[index]) ++index;
    if (index==0 || ++index>=length) return 0;
    uint32_t name_length=index-1;
    if (name_length>sizeof(file_name)-1) name_length=sizeof(file_name)-1;
    memcpy(file_name, data, name_length);
    file_name[name_length]=0;
    uint32_t first=index;
    while (index<length && data[index]>='0' && data[index]<='9') {
        uint32_t digit=data[index++]-'0';
        if (value>(BOOT_APP_SIZE-digit)/10U) return 0;
        value=value*10U+digit;
    }
    if (index==first || index>=length || (data[index]!=0 && data[index]!=' ')) return 0;
    *size=value;
    return value>=8 && value<=BOOT_APP_SIZE;
}
/* 单文件接收状态机。
 * started 区分首包等待与传输中超时；eot_seen 区分数据序号回绕后的 0 包
 * 和传输结束的空 0 包。只有收到声明长度的全部数据才接受 EOT。
 * 成功返回 1；失败返回 0，receive_result 用于输出旧版错误文本。
 */
static int download(void) {
    uint32_t size=0, received=0;
    uint8_t sequence=1;
    unsigned errors=0;
    int started=0, eot_seen=0;
    receive_result=RECEIVE_ERROR;
    image_size=0;
    /* 旧上位机收到 Waiting 后开始握手；首次 C 在 2 秒超时后发出。
     * 首包前不累计超时，允许操作者准备文件；传输开始后累计 6 次退出。
     */
    while (errors<6) {
        int length=receive_packet();
        if (length==-2) {
            receive_result=RECEIVE_ABORT;
            send_byte(ACK);
            return 0;
        }
        if (length==-3) {
            receive_result=RECEIVE_ABORT;
            goto fail;
        }
        if (length<0) {
            if (started && ++errors>=6) goto fail;
            send_byte(CRC_REQUEST);
            continue;
        }
        if (length==0) {
            if (!started || received!=size) goto fail;
            if (++errors>=6) goto fail;
            /* 兼容关键：第一次 EOT 直接 ACK，不能改为双 EOT 的 NAK 流程。
             * 随后接受空 0 包；若主机等待 C，由超时分支发出 C。
             */
            eot_seen=1;
            send_byte(ACK);
            continue;
        }
        if (eot_seen && packet[0]==0 && packet[2]==0) {
            if (!image_valid()) { receive_result=RECEIVE_DATA; goto fail; }
            send_byte(ACK);
            return 1;
        }
        if (!started) {
            if (packet[0]!=0 || !header_size((uint32_t)length, &size)) {
                receive_result=RECEIVE_LIMIT;
                goto fail;
            }
            image_size=size;
            if (!erase_app()) { receive_result=RECEIVE_DATA; goto fail; }
            started=1;
            errors=0;
            send_byte(ACK);
            send_byte(CRC_REQUEST);
        } else if (packet[0]==sequence && received<size && !eot_seen) {
            uint32_t count=size-received;
            if (count>(uint32_t)length) count=(uint32_t)length;
            if (!write_app(BOOT_APP_ADDRESS+received, packet+2, count)) {
                receive_result=RECEIVE_DATA;
                goto fail;
            }
            received+=count;
            ++sequence; /* 序号按 uint8_t 在 255 后回到 0，不能当成新文件头。 */
            errors=0;
            send_byte(ACK);
        } else if (packet[0]==(uint8_t)(sequence-1U) && !eot_seen) {
            /* ACK 丢失导致的重传只重发 ACK，避免 Flash 同一双字二次编程。 */
            ++errors;
            send_byte(ACK);
            if (received==0) send_byte(CRC_REQUEST);
        } else { ++errors; send_byte(NAK); }
    }
fail:
    send_byte(YM_CANCEL);
    send_byte(YM_CANCEL);
    return 0;
}
/* 切换 MSP 后禁止再访问 Boot 的 C 栈，因此末段使用 naked 汇编跳转。 */
__attribute__((naked, noreturn)) static void enter_app(uint32_t stack, uint32_t entry) {
    __asm volatile("msr msp, r0\nmovs r2, #0\nmsr control, r2\nisb\ncpsie i\nbx r1\n");
}
/* 清理 Boot 的串口、时钟、SysTick 和挂起中断，再切换向量表及栈。
 * APP 的链接起点与 SystemInit 向量表偏移必须同为 0x08008000。
 */
static void jump_app(void) {
    uint32_t stack=*(volatile uint32_t *)BOOT_APP_ADDRESS;
    uint32_t entry=*(volatile uint32_t *)(BOOT_APP_ADDRESS+4);
    HAL_UART_DeInit(port);
    HAL_RCC_DeInit();
    HAL_DeInit();
    __disable_irq();
    SysTick->CTRL=0;
    SysTick->LOAD=0;
    SysTick->VAL=0;
    for (unsigned i=0; i<8; ++i) {
        NVIC->ICER[i]=0xFFFFFFFFUL;
        NVIC->ICPR[i]=0xFFFFFFFFUL;
    }
    SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk|SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR=BOOT_APP_ADDRESS;
    __DSB();
    __ISB();
    enter_app(stack, entry);
}
void Boot_Run(UART_HandleTypeDef *uart) {
    port=uart;
    /* 升级期间将阀门 PWM 引脚 PA8 保持低电平。 */
    GPIO_InitTypeDef gpio={0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    gpio.Pin=GPIO_PIN_8;
    gpio.Mode=GPIO_MODE_OUTPUT_PP;
    gpio.Pull=GPIO_NOPULL;
    gpio.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    message("Press abc to interrupt ......\r\n\n");
    /* 滑动匹配：前面的无关字符不影响后续 abc 命中。 */
    const char trigger[]="abc";
    unsigned matched=0;
    uint32_t start=HAL_GetTick();
    while (HAL_GetTick()-start<5000U) {
        uint8_t value;
        if (HAL_UART_Receive(port, &value, 1, 10) != HAL_OK) continue;
        matched=value==(uint8_t)trigger[matched] ? matched+1 : value=='a' ? 1 : 0;
        if (matched==3) break;
    }
    if (matched!=3 && image_valid()) {
        message("Jump to App...\r\n");
        jump_app();
    }
    for (;;) {
        /* Porbe 的拼写和所有提示换行均保留，供旧上位机识别。 */
        message("  Download Image To the Porbe Internal Flash ----------- 1\r\n\n");
        uint8_t key=0;
        do { HAL_UART_Receive(port, &key, 1, 1000); } while (key!='1');
        message("Waiting  ... \n\r");
        if (download()) {
            char number[11]={0};
            uint32_t divider=1000000000U, pos=0;
            for (; divider; divider/=10U) {
                uint32_t digit=image_size/divider%10U;
                if (digit || pos || divider==1) number[pos++]=(char)('0'+digit);
            }
            message("\n\n\r Programming Completed Successfully!\n\r--------------------------------\r\n Name: ");
            message(file_name);
            message("\n\r Size: ");
            message(number);
            message(" Bytes\r\n");
            message("-------------------\n");
            HAL_Delay(1000);
            NVIC_SystemReset();
        }
        switch (receive_result) {
        case RECEIVE_LIMIT:
            message("\n\n\rThe image size is higher than the allowed space memory!\n\r");
            break;
        case RECEIVE_DATA:
            message("\n\n\rVerification failed!\n\r");
            break;
        case RECEIVE_ABORT:
            message("\r\n\nAborted by user.\n\r");
            break;
        default:
            message("\n\rFailed to receive the file!\n\r");
            break;
        }
    }
}

"""Exercise actual Boot protocol functions with UART and Flash mocks.

Hardware-only functions are excluded; this does not replace board testing.
Requires a native gcc compiler.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[1]
source = (repo / "boot/l432boot/Core/Src/boot.c").read_text(encoding="utf-8")
header = (repo / "boot/l432boot/Core/Inc/boot.h").read_text(encoding="utf-8")

def function(name):
    start = re.search(r"static (?:int|void|uint16_t) " + name + r"\(", source).start()
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

preamble = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef struct {uint32_t ErrorCode, RxXferCount;} UART_HandleTypeDef;
typedef int HAL_StatusTypeDef;
static UART_HandleTypeDef fake_port;
static unsigned recoveries;
static void recover_rx(void) { ++recoveries; }
#define HAL_OK 0
static uint8_t input[120000], output[10000], flash[100000];
static unsigned in_size, in_pos, out_size, written, erases;
static int erase_ok, valid;
static int HAL_UART_Receive(void *p, uint8_t *d, uint16_t n, uint32_t t) {
    (void)p; (void)t;
    if (in_pos+n > in_size) { ((UART_HandleTypeDef *)p)->RxXferCount=n; return 1; }
    memcpy(d,input+in_pos,n); in_pos+=n; return HAL_OK;
}
static int HAL_UART_Transmit(void *p, uint8_t *d, uint16_t n, uint32_t t) {
    (void)p; (void)t; assert(out_size+n < sizeof(output));
    memcpy(output+out_size,d,n); out_size+=n; return HAL_OK;
}
static int erase_app(void) { ++erases; return erase_ok; }
static int image_valid(void) { return valid; }
static int write_app(uint32_t a, const uint8_t *d, uint32_t n) {
    assert(a == BOOT_APP_ADDRESS+written);
    assert(written+n <= BOOT_APP_SIZE);
    memcpy(flash+written,d,n); written+=n; return 1;
}
'''
tests = r'''
static void reset(void) {
    in_size=in_pos=out_size=written=erases=0; erase_ok=valid=1;
}
static void add_packet(uint8_t seq, const uint8_t *data, unsigned n) {
    input[in_size++]=n==128 ? SOH : STX;
    input[in_size++]=seq; input[in_size++]=(uint8_t)~seq;
    memcpy(input+in_size,data,n); in_size+=n;
    uint16_t crc=crc16(data,n);
    input[in_size++]=(uint8_t)(crc>>8); input[in_size++]=(uint8_t)crc;
}
static void add_header(unsigned size) {
    uint8_t h[128]={0}; memcpy(h,"app.bin",7);
    snprintf((char *)h+8,100,"%u",size); add_packet(0,h,128);
}
static void finish(void) {
    uint8_t empty[128]={0};
    input[in_size++]=EOT; input[in_size++]=EOT; add_packet(0,empty,128);
}
int main(void) {
    port=&fake_port;
    uint8_t data[1024]; memset(data,0x5a,sizeof(data));
    assert(crc16((const uint8_t *)"123456789",9)==0x31c3);
    /* Legacy host: a single EOT is ACKed, then empty header is ACKed. */
    reset(); add_header(128); add_packet(1,data,128);
    input[in_size++]=EOT;
    uint8_t empty[128]={0}; add_packet(0,empty,128);
    assert(download()==1 && image_size==128 && strcmp(file_name,"app.bin")==0);
    const uint8_t legacy_reply[]={ACK,CRC_REQUEST,ACK,ACK,ACK};
    assert(out_size==sizeof(legacy_reply) && memcmp(output,legacy_reply,out_size)==0);
    /* Host waits for C after EOT: timeout requests the final empty header. */
    reset(); input[in_size++]=0xff; add_header(128); add_packet(1,data,128);
    input[in_size++]=EOT; input[in_size++]=0xff; add_packet(0,empty,128);
    assert(download()==1);
    const uint8_t delayed_reply[]={CRC_REQUEST,ACK,CRC_REQUEST,ACK,ACK,CRC_REQUEST,ACK};
    assert(out_size==sizeof(delayed_reply) && memcmp(output,delayed_reply,out_size)==0);
    reset(); add_header(129); add_packet(1,data,128); add_packet(1,data,128);
    add_packet(2,data,128); finish();
    assert(download()==1 && written==129 && erases==1);
    assert(flash[128]==0x5a); /* final padding excluded */
    reset(); add_header(BOOT_APP_SIZE+1); assert(download()==0 && erases==0);
    reset(); add_header(0); assert(download()==0 && erases==0);
    reset(); add_header(128); erase_ok=0; assert(download()==0 && written==0);
    reset(); add_header(256); add_packet(1,data,128); finish();
    assert(download()==0 && written==128); /* premature EOT */
    reset(); add_header(128); add_packet(1,data,128); input[in_size-1]^=1;
    add_packet(1,data,128); finish(); assert(download()==1 && written==128);
    assert(boot_rx_diagnostic.reason==5 && recoveries>0);
    reset(); add_header(128); input[in_size++]=STX;
    assert(download()==0 && boot_rx_diagnostic.reason==3);
    assert(boot_rx_diagnostic.expected==1028 && boot_rx_diagnostic.received==0);
    reset(); add_header(128); add_packet(1,data,128); valid=0; finish();
    assert(download()==0);
    reset(); add_header(128); /* no data: bounded timeout */
    assert(download()==0 && out_size<30);
    reset(); input[in_size++]=YM_CANCEL; input[in_size++]=YM_CANCEL;
    assert(download()==0 && erases==0);
    assert(out_size==1 && output[0]==ACK && receive_result==RECEIVE_ABORT);
    reset(); input[in_size++]='a'; assert(download()==0 && erases==0);
    assert(out_size==2 && output[0]==YM_CANCEL && output[1]==YM_CANCEL);
    reset(); add_header(BOOT_APP_SIZE);
    for (unsigned i=0; i<BOOT_APP_SIZE/128; ++i) add_packet((uint8_t)(i+1),data,128);
    finish(); assert(download()==1 && written==BOOT_APP_SIZE);
    reset(); add_header(1024); add_packet(0, input+3,128);
    add_packet(1,data,1024); finish(); assert(download()==1 && erases==1);
    puts("Boot protocol: legacy single-EOT ACK/C handshake, cancellation, CRC, retry, padding, limits, erase failure, truncation, timeout and sequence wrap passed.");
}
'''
compiler = shutil.which("gcc")
if not compiler:
    raise SystemExit("Native gcc is required on PATH")
with tempfile.TemporaryDirectory(prefix="boot-tests-") as folder:
    path = Path(folder)
    defines = "\n".join(line for line in header.splitlines() if line.startswith("#define BOOT_APP_"))
    globals_ = source[source.index("enum {"):source.index("static void recover_rx")]
    code = "#include <stdint.h>\n" + defines + preamble + globals_ + "\n".join(
        function(name) for name in ("send_byte", "crc16", "receive_packet", "header_size", "download")) + tests
    (path / "test.c").write_text(code, encoding="utf-8")
    exe = path / "test.exe"
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", str(path / "test.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

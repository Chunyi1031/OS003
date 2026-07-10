#include <klib.h>
#include <Time.h>

//获取CPU频率
uint64_t Get_CPU_Frequency(){
    struct rtc_time time;
    //等待下一个整秒
    get_rtc_time(&time);
    uint16_t s1 = time.second;
    uint16_t s2 = 0;
    while (s2 != (s1 + 1)){
        get_rtc_time(&time);
        s2 = time.second;
        asm("pause");
    }
    uint64_t tsc1 = rdtsc();//第一次获取CPU振荡次数
    //等待下一秒
    uint16_t s3 = 0;
    while (s3 != (s2 + 1)){
        get_rtc_time(&time);
        s3 = time.second;
        asm("pause");
    }
    uint64_t tsc2 = rdtsc();//第二次
    return tsc2 - tsc1;//两次相距1秒，两次的相减即为频率
}

//延迟毫秒
void delay_ms(uint32_t ms) {
    uint64_t cycles_to_wait = (uint64_t)ms * (SYSTEM_CPU_Frequency / 1000);//计算需要等待的总周期数
    uint64_t start_tsc = rdtsc();//记录起始点
    //循环检查经过的周期数是否达到目标
    while ((rdtsc() - start_tsc) < cycles_to_wait) {
        asm volatile("pause");//pause降低功耗
    }
}
//延迟秒
void delay_seconds(uint32_t seconds){
    delay_ms(seconds * 1000);
}

#include <Keyboard.h>
void SYSTEM_DEBUG(){
    asm("cli");
    while(!kbhit());
    asm("sti");
    delay_ms(800);
}
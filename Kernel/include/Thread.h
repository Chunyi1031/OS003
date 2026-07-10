/**
 * OS003 线程管理
 * 2026/7/10 Liu Chunyi
 */

#ifndef _THREAD_H_
#define _THREAD_H_

#include <klib.h>
#include <mm/vmm.h>
#include <Interrupt.h>
#include <Syscall.h>

// 线程状态
#define THREAD_READY      0
#define THREAD_RUNNING    1
#define THREAD_BLOCKED    2
#define THREAD_TERMINATED 3

#define THREAD_PRIV_KERNEL 0//Ring 0
#define THREAD_PRIV_USER   3//Ring 3

#define FREE_TCB_LIST_SIZE 4
#define TORT_SIZE          16//正在运行的线程的表的大小

//线程上下文
typedef struct thread_context {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) thread_context_t;

// 线程控制块
typedef struct thread {
    thread_context_t context;
    uint64_t id;                //线程 ID
    uint8_t state;              //状态
    void *kernel_stack;         //内核栈基址（用于分配）
    uint64_t stack_size;        //栈大小
    struct thread *next;        //就绪队列链表指针
    int PositionInTable;        //线程在任务表中的位置
    uint64_t wakeup_time;       //唤醒时间
    _Bool use_vmm;              //是否使用虚拟内存管理器
    VMM_T* vmm;                 //虚拟内存管理器
    void *user_entry;           //Ring 3 入口点（仅用户线程使用）
    void *user_rsp;             //Ring 3 栈指针（仅用户线程使用）
    uintptr_t stack;            //栈地址（仅记录）
}__attribute__((packed)) thread_t;

//自旋锁类型
typedef struct spinlock {
    volatile uint8_t locked;  //0: 未锁, 1: 已锁
} spinlock_t;

//互斥量结构体
typedef struct mutex {
    volatile uint8_t locked;           //0=未锁定，1=已锁定
    thread_t *owner;                   //当前持有锁的线程
    thread_t *wait_queue_head;         //等待队列头
    thread_t *wait_queue_tail;         //等待队列尾
    volatile uint32_t lock_count;      //锁计数器，用于递归锁
} mutex_t;

//信号量结构体
typedef struct semaphore {
    volatile int count;              //信号量计数
    thread_t *wait_queue_head;       //等待队列头
    thread_t *wait_queue_tail;       //等待队列尾
} semaphore_t;

// 初始化自旋锁
#define SPINLOCK_INIT {0}
static inline void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

extern thread_t *current_thread;//全局当前运行线程
extern thread_t *SYSTEM_KernelThread;//全局内核线程

/**
 * @brief 创建内核线程
 * @param entry 任务函数指针
 * @param stack_size 栈大小（字节）
 * @return 线程TCB
 */
thread_t *CreateKernelThread(void (*entry)(void), uint64_t stack_size);
/**
 * @brief 创建用户线程
 * @param entry 任务函数指针
 * @param stack_size 栈大小（字节）
 * @return 线程TCB
 */
thread_t *CreateUserThread(void (*entry)(void), uint64_t stack_size);
/**
 * @brief 结束线程
 * @param thread 线程TCB
 */
void KillThread(thread_t* thread);
void ThreadSleepMS(uint64_t ms);//休眠指定毫秒数
void ThreadSleepSecond(uint64_t s);//休眠指定秒数

thread_t *create_user_thread(void (*entry)(void), void *stack, uint64_t stack_size, VMM_T* vmm);
void user_thread_bootstrap(void) __attribute__((noreturn));
void init_multitasking(void);//初始化多任务
void thread_enqueue(thread_t *thread);
thread_t *create_thread(void (*entry)(void), void *stack,uint64_t stack_size,_Bool UseVMM,VMM_T* vmm);//创建一个线程
thread_t *thread_dequeue(void);
void thread_exit(void);
void switch_to(void *prev, void *next,uintptr_t new_cr3);//切换线程
void schedule(void);
void spinlock_acquire(spinlock_t *lock);// 获取自旋锁
void spinlock_release(spinlock_t *lock);// 释放自旋锁
STATUS spinlock_try_acquire(spinlock_t *lock);// 尝试获取锁（非阻塞），成功返回1，失败返回0
void mutex_init(mutex_t *mutex);//初始化互斥量
void mutex_lock(mutex_t *mutex);//获取互斥量
void mutex_unlock(mutex_t *mutex);//释放互斥量
int mutex_is_owned_by_current(mutex_t *mutex); //检查当前线程是否拥有互斥量
void sem_init(semaphore_t *sem, int value);//初始化信号量
void sem_wait(semaphore_t *sem);//P操作（等待）
void sem_post(semaphore_t *sem);//V操作（发送信号）
void PrintRunningThreads();//打印正在运行的线程
void wake_sleeping_threads(void); // 唤醒睡眠线程
void CleanAllThreads();//清理所有进程

#endif
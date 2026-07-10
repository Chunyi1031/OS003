#include <Thread.h>
#include <Memory.h>
#include <Print.h>
#include <int/gdt.h>
#include <mm/paging.h>

thread_t *current_thread = NULL;
static uint64_t next_thread_id = 1;
static int next_TORT_Position = 0;
static uintptr_t free_tcb_list[FREE_TCB_LIST_SIZE]; //空闲TCB链表
static uint32_t num_of_FreeTCB = 0;
static uintptr_t TORT[TORT_SIZE];//正在运行的线程的表

// 睡眠队列
static thread_t *sleep_queue_head = NULL;
static thread_t *sleep_queue_tail = NULL;
// 就绪队列头指针
static thread_t *ready_queue_head = NULL;
static thread_t *ready_queue_tail = NULL;  // 方便尾部插入

void idle_thread(void) {
    while(1);
}

// 初始化多任务环境（创建 idle 线程或主线程）
void init_multitasking(void) {
    memset(TORT, 0, sizeof(TORT));//清空TORT
    memset(free_tcb_list, 0, sizeof(free_tcb_list));//清空空闲TCB链表
    // 初始化队列头指针
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    sleep_queue_head = NULL;
    sleep_queue_tail = NULL;
    current_thread = (thread_t *)Pmm_Malloc(1);
    memset(current_thread, 0, sizeof(thread_t));
    current_thread->id = 0;
    current_thread->state = THREAD_RUNNING;
    current_thread->kernel_stack = SYSTEM_BootShare->MemoryMap->StackAddr;
    current_thread->use_vmm = false;
    current_thread->vmm = Current_VMM_Desc;
    SYSTEM_KernelThread = current_thread;
    // 设置 TSS.RSP0：Ring 3→Ring 0 中断/异常时 CPU 使用这个栈
    tss_set_rsp0((uint64_t)current_thread->kernel_stack + 4096);
    CreateKernelThread(idle_thread, 4096);
}

void thread_enqueue(thread_t *thread) {
    thread->next = NULL;
    if (!ready_queue_head) {
        ready_queue_head = ready_queue_tail = thread;
    } else {
        ready_queue_tail->next = thread;
        ready_queue_tail = thread;
    }
    thread->state = THREAD_READY;
}

thread_t *thread_dequeue(void) {
    if (!ready_queue_head) return NULL;
    thread_t *thread = ready_queue_head;
    ready_queue_head = ready_queue_head->next;
    if (!ready_queue_head) ready_queue_tail = NULL;
    thread->next = NULL;
    return thread;
}

// 睡眠队列专用的入队函数
void sleep_enqueue(thread_t *thread) {
    thread->next = NULL;
    if (!sleep_queue_head) {
        sleep_queue_head = sleep_queue_tail = thread;
    } else {
        sleep_queue_tail->next = thread;
        sleep_queue_tail = thread;
    }
}

// 睡眠队列专用的出队函数
thread_t *sleep_dequeue(void) {
    if (!sleep_queue_head) return NULL;
    thread_t *thread = sleep_queue_head;
    sleep_queue_head = sleep_queue_head->next;
    if (!sleep_queue_head) sleep_queue_tail = NULL;
    thread->next = NULL;
    return thread;
}

// 从睡眠队列中移除指定线程
thread_t *remove_from_sleep_queue(thread_t *target) {
    if (!sleep_queue_head) return NULL;
    if (sleep_queue_head == target) {
        return sleep_dequeue();
    }
    
    thread_t *curr = sleep_queue_head;
    while (curr && curr->next != target) {
        curr = curr->next;
    }
    
    if (curr && curr->next == target) {
        thread_t *removed = target;
        curr->next = target->next;
        if (target == sleep_queue_tail) {
            sleep_queue_tail = curr;
        }
        removed->next = NULL;
        return removed;
    }
    return NULL;
}

thread_t *schedule_prev_thread = NULL;

__attribute__((__externally_visible__))
void thread_exit(void) {
    disable_interrupts();
    current_thread->state = THREAD_TERMINATED;//标记为终止
    remove_from_sleep_queue(current_thread);//从睡眠队列中移除（如果在睡眠队列中）
    schedule();//调度其他线程
    SYSTEM_STOP();
}

// int $0x80 处理函数：Ring 3 线程结束时调用
void user_thread_exit_cb(void) {
    thread_exit();
}

thread_t *create_thread(void (*entry)(void), void *stack,uint64_t stack_size,_Bool UseVMM,VMM_T* vmm){
    disable_interrupts();//关中断保护
    thread_t *thread = NULL;
    //如果链表有空闲的 TCB，则从链表中获取
    if (free_tcb_list[num_of_FreeTCB] != 0) {
        thread = (thread_t*)free_tcb_list[num_of_FreeTCB];//设置TCB
        memset(thread, 0, sizeof(thread_t));//清空
        free_tcb_list[num_of_FreeTCB] = 0;//已被占用，把该地址清除
        num_of_FreeTCB --;//回退
    //否则
    } else {
        thread = (thread_t *)Pmm_Malloc(1);//再开辟一个TCB
    }
    if (!thread)return NULL;
    if (!stack) {
        Pmm_Free(thread, 1);
        return NULL;
    }
    memset(thread, 0, sizeof(thread_t));
    // 初始化 TCB
    thread->id = next_thread_id++;
    thread->state = THREAD_READY;
    thread->stack_size = stack_size;
    thread->kernel_stack = stack;
    thread->stack = (uintptr_t)stack;
    thread->use_vmm = UseVMM;
    thread->vmm = vmm;
    if(next_TORT_Position < TORT_SIZE){
        thread->PositionInTable = next_TORT_Position;
        TORT[next_TORT_Position] = (uintptr_t)thread;//加入表
        next_TORT_Position ++;//下一个位置
    }
    VMM_T* old_vmm;
    if(thread->use_vmm){
        old_vmm = (VMM_T*)Switch_Virtual_Mamanager(vmm);
    }
    uint64_t stack_bottom = (uint64_t)stack + stack_size;// 计算栈底（高地址）
    // 预留 18 个槽位（17 个寄存器 + rip），并 16 字节对齐
    uint64_t stack_frame = ((stack_bottom - 18 * 8) & ~15ULL);
    uint64_t *frame = (uint64_t *)stack_frame;
    // 初始化栈帧：所有通用寄存器初始为 0，rflags = 0x202，rip = entry
    memset(frame, 0, 18 * 8);
    frame[15] = 0x202;          // rflags
    frame[16] = (uint64_t)entry; // rip
    frame[17] = (uint64_t)thread_exit; // 返回地址
    thread->context.rsp = stack_frame;// 设置线程上下文的 rsp
    if(thread->use_vmm){
        Switch_Virtual_Mamanager(old_vmm);
    }
    thread_enqueue(thread);
    enable_interrupts();
    return thread;
}

//创建一个内核线程
thread_t *CreateKernelThread(void (*entry)(void), uint64_t stack_size) {
    //分配内核栈
    stack_size = (stack_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return create_thread(entry, Pmm_Malloc(stack_size / PAGE_SIZE) ,stack_size,false,Current_VMM_Desc);
}

//Ring 3 用户线程：bootstrap 用 iretq 进入 Ring3
__attribute__((noreturn)) void user_thread_bootstrap(void) {
    uint64_t user_entry = 0;
    uint64_t user_sp = 0;
    disable_interrupts();//关中断，防止在读取 current_thread 时被调度
    user_entry = (uint64_t)current_thread->user_entry;
    user_sp   = (uint64_t)current_thread->user_rsp;
    // iretq 帧 + 返回地址槽 = 48 字节
    // 布局（RSP 指帧底）：
    // [RSP+0]:   RIP (用户函数)
    // [RSP+8]:   CS (0x1B)
    // [RSP+16]:  RFLAGS
    // [RSP+24]:  outer RSP = user_sp - 8
    // [RSP+32]:  outer SS (0x23)
    // [RSP+40]:  返回地址（hlt 自旋）← RSP after iretq
    // 函数返回时 ret 弹 [RSP+40] 跳转到 hlt 自旋，防止 #PF
    uint64_t ret_addr = (user_entry & ~0xFFFULL) + PAGE_SIZE - 8;
    __asm__ volatile(
        "movq %[user_sp], %%rsp\n"
        "subq $48, %%rsp\n"                  // 6 个槽
        "movq %[entry], 0(%%rsp)\n"          // RIP
        "movq %[cs], 8(%%rsp)\n"             // CS
        "pushfq\n"
        "popq %%rax\n"
        "orq $0x200, %%rax\n"
        "movq %%rax, 16(%%rsp)\n"            // RFLAGS
        "leaq -8(%[user_sp]), %%rax\n"
        "movq %%rax, 24(%%rsp)\n"            // outer RSP = user_sp - 8
        "movq %[ss], 32(%%rsp)\n"             // outer SS
        "movq %[ret], 40(%%rsp)\n"            // 返回地址（hlt 自旋）
        "iretq\n"
        :
        : [entry] "r"(user_entry),
          [user_sp] "r"(user_sp),
          [cs] "r"((uint64_t)USER_CS),
          [ss] "r"((uint64_t)USER_DS),
          [ret] "r"(ret_addr)
        : "rax", "memory"
    );
    __builtin_unreachable();
}

thread_t *create_user_thread(void (*entry)(void), void *stack, uint64_t stack_size, VMM_T* vmm) {
    disable_interrupts();//关中断
    //设置TCB
    thread_t *thread = (thread_t *)Pmm_Malloc(1);
    if (!thread) return NULL;
    memset(thread, 0, sizeof(thread_t));
    //分配内核栈
    void* kernel_stack = Pmm_Malloc(1);
    if(!kernel_stack){
        Pmm_Free(thread,1);
        return NULL;
    }
    thread->id = next_thread_id++;
    thread->state = THREAD_READY;
    thread->stack_size = stack_size;
    thread->kernel_stack = kernel_stack;
    thread->stack = (uintptr_t)stack;
    thread->use_vmm = true;
    thread->vmm = vmm;
    //设置用户入口和用户栈顶
    thread->user_entry = entry;
    thread->user_rsp = (void*)((uint64_t)stack + stack_size);
    if (next_TORT_Position < TORT_SIZE) {
        thread->PositionInTable = next_TORT_Position;
        TORT[next_TORT_Position] = (uintptr_t)thread;
        next_TORT_Position++;
    }
    VMM_T* old_vmm = (VMM_T*)Switch_Virtual_Mamanager(vmm);//切换虚拟内存管理器
    // rip 指向 user_thread_bootstrap，bootstrap 会用 iretq 跳入 Ring 3
    uint64_t stack_bottom = (uint64_t)kernel_stack + stack_size;
    uint64_t stack_frame = ((stack_bottom - 18 * 8) & ~15ULL);
    uint64_t *frame = (uint64_t *)stack_frame;
    memset(frame, 0, 18 * 8);
    frame[15] = 0x202;//rflags
    frame[16] = (uint64_t)user_thread_bootstrap;//rip → bootstrap (非 entry!)
    frame[17] = (uint64_t)thread_exit;//返回地址（理论上不会用到）
    thread->context.rsp = stack_frame;
    Switch_Virtual_Mamanager(old_vmm);
    thread_enqueue(thread);
    enable_interrupts();
    return thread;
}

//创建用户线程
thread_t *CreateUserThread(void (*entry)(void), uint64_t stack_size) {
    stack_size = (stack_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    VMM_T* vmm = Init_User_Virtual_Memory_Manager(PTE_PRESENT | PTE_WRITABLE | PTE_USER);//创建专用用户态虚拟内存
    if (!vmm) return NULL;
    VMM_T* old_vmm = (VMM_T*)Switch_Virtual_Mamanager(vmm);//切换虚拟内存管理器
    // 把入口代码拷贝到 VMM 自身可写的虚拟页
    uint64_t entry_page = (uint64_t)entry & PAGE_MASK;
    uint64_t entry_off  = (uint64_t)entry & 0xFFF;
    //在VMM空间中分配一页，把代码拷贝过来
    void* code_copy = Vmm_Malloc(PAGE_SIZE);
    if (!code_copy) {
        Switch_Virtual_Mamanager(old_vmm);
        return NULL;
    }
    memcpy(code_copy, (void*)entry_page, PAGE_SIZE);
    uint8_t* trampoline = (uint8_t*)code_copy + PAGE_SIZE - 8;
    //int $0x80
    trampoline[0] = 0xCD;
    trampoline[1] = 0x80;
    void* new_entry = (void*)((uint64_t)code_copy | entry_off);
    void* user_stack = Vmm_Malloc(stack_size);//分配栈
    Switch_Virtual_Mamanager(old_vmm);
    if (!user_stack) {
        return NULL;
    }
    return create_user_thread(new_entry, user_stack, stack_size, vmm);//创建
}

__attribute__((naked))
void switch_to(void *prev, void *next,uintptr_t new_cr3) {
    __asm__ volatile (
        // 保存当前寄存器到 prev 的栈
        "pushfq\n"
        "pushq %%rax\n"
        "pushq %%rcx\n"
        "pushq %%rdx\n"
        "pushq %%rbx\n"
        "pushq %%rbp\n"
        "pushq %%rsi\n"
        "pushq %%rdi\n"
        "pushq %%r8\n"
        "pushq %%r9\n"
        "pushq %%r10\n"
        "pushq %%r11\n"
        "pushq %%r12\n"
        "pushq %%r13\n"
        "pushq %%r14\n"
        "pushq %%r15\n"

        // 保存当前栈指针到 prev->context.rsp
        "movq %%rsp, (%%rdi)\n"

        // 先读取 next 的 rsp（在 CR3 切换之前，因为 next 可能位于旧 VMM 地址空间）
        "movq (%%rsi), %%rsp\n"

        // 再切换 CR3（如果提供了新页表）
        "movq %[cr3], %%rdx\n"
        "test %%rdx, %%rdx\n"
        "jz 1f\n"
        "movq %%rdx, %%cr3\n"
        "1:\n"

        // 从新栈恢复所有寄存器
        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%r11\n"
        "popq %%r10\n"
        "popq %%r9\n"
        "popq %%r8\n"
        "popq %%rdi\n"
        "popq %%rsi\n"
        "popq %%rbp\n"
        "popq %%rbx\n"
        "popq %%rdx\n"
        "popq %%rcx\n"
        "popq %%rax\n"
        "popfq\n"

        "ret\n"
        :
        : "D"(prev), "S"(next), [cr3] "r"(new_cr3)
        : "memory"
    );
}

__attribute__((__externally_visible__))
void schedule(void) {
    // 如果当前线程正在运行，放回就绪队列
    if (current_thread && current_thread->state == THREAD_RUNNING) {
        current_thread->state = THREAD_READY;//状态改为就绪
        thread_enqueue(current_thread);//添加到就绪队列
    }
    wake_sleeping_threads();//唤醒到达时间的睡眠线程
    thread_t *next = thread_dequeue();//从就绪队列中取出一个线程作为下一个线程
    //如果出错，则调度内核主线程，防止连累整个系统
    if (!next) {
        next = SYSTEM_KernelThread;
        if (!next) {
            return;
        }
        //确保主线程状态正确
        if (next->state != THREAD_RUNNING)
            next->state = THREAD_RUNNING;
    }
    schedule_prev_thread = current_thread;//保存当前线程为上一个线程
    current_thread = next;//设置当前线程为下一个线程
    current_thread->state = THREAD_RUNNING;//设置当前线程状态为运行中
    //如果线程改变，切换线程
    if (schedule_prev_thread != next) {
        uint64_t target_cr3 = 0;
        if (current_thread->use_vmm) {
            target_cr3 = (uintptr_t)current_thread->vmm->Pml4;
        } else if (schedule_prev_thread->use_vmm) {
            target_cr3 = (uintptr_t)KernelPML4;
        }
        // 设置 TSS.RSP0：确保当前线程的内核栈作为 Ring 3→Ring 0 的 fallback 栈
        // 值写的是栈顶（高地址），因为栈向下生长
        tss_set_rsp0((uint64_t)current_thread->kernel_stack + current_thread->stack_size);
        syscall_cpu.kernel_rsp = (uint64_t)current_thread->kernel_stack + current_thread->stack_size;
        switch_to(&schedule_prev_thread->context.rsp, &next->context.rsp,target_cr3);
        if(target_cr3)Current_VMM_Desc = current_thread->vmm;//更新当前VMM描述符
    }
    if(schedule_prev_thread->state == THREAD_TERMINATED){
        KillThread(schedule_prev_thread);
    }
    schedule_prev_thread = NULL;
}

// 获取自旋锁
void spinlock_acquire(spinlock_t *lock) {
    // 关中断以防止死锁（如果当前线程持有锁时被中断，中断处理程序也可能尝试获取同一把锁）
    disable_interrupts();

    // 使用 xchg 指令原子性地交换
    asm volatile (
        "1: xchgb %0, %1\n"
        "   testb %0, %0\n"
        "   jz 2f\n"
        "   pause\n"
        "   jmp 1b\n"
        "2:\n"
        : "+r" (*(uint8_t*)&lock->locked), "+m" (lock->locked)
        :
        : "memory"
    );
}

// 释放自旋锁
void spinlock_release(spinlock_t *lock) {
    // 使用 xchg 清0
    asm volatile (
        "xchgb %0, %1\n"
        : "+r" (*(uint8_t*)&lock->locked), "+m" (lock->locked)
        :
        : "memory"
    );
    
    // 重新开启中断
    enable_interrupts();
}

// 尝试获取锁
STATUS spinlock_try_acquire(spinlock_t *lock) {
    uint8_t old = 1;
    asm volatile (
        "xchgb %0, %1\n"
        : "+r" (old), "+m" (lock->locked)
        :
        : "memory"
    );
    return old == 0;  // 如果原来为0，则获取成功
}

static void thread_enqueue_wait(mutex_t *mutex, thread_t *thread) {
    thread->next = NULL;
    if (!mutex->wait_queue_head) {
        mutex->wait_queue_head = mutex->wait_queue_tail = thread;
    } else {
        mutex->wait_queue_tail->next = thread;
        mutex->wait_queue_tail = thread;
    }
}

static thread_t *thread_dequeue_wait(mutex_t *mutex) {
    if (!mutex->wait_queue_head) return NULL;
    thread_t *thread = mutex->wait_queue_head;
    mutex->wait_queue_head = thread->next;
    if (!mutex->wait_queue_head) mutex->wait_queue_tail = NULL;
    thread->next = NULL;
    return thread;
}

void mutex_init(mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner = NULL;
    mutex->wait_queue_head = mutex->wait_queue_tail = NULL;
}

void mutex_lock(mutex_t *mutex) {
    disable_interrupts();   // 关中断，保证原子性
    
    // 检查是否为递归锁（同一线程再次获取锁）
    if (mutex->owner == current_thread) {
        // 如果是递归锁，可以增加计数器（这里简化处理，只做基本检查）
        enable_interrupts();
        return;
    }
    
    while (mutex->locked) {
        // 锁已被占用，当前线程进入等待队列并让出CPU
        current_thread->state = THREAD_BLOCKED;
        thread_enqueue_wait(mutex, current_thread);
        schedule();          // 切换出去，返回时中断仍为关
        // 被唤醒后，重新检查锁状态（可能又被其他线程抢走，故用while循环）
    }
    // 成功获得锁
    mutex->locked = 1;
    mutex->owner = current_thread;
    enable_interrupts();
}

void mutex_unlock(mutex_t *mutex) {
    disable_interrupts();
    
    // 检查是否当前线程拥有锁
    if (mutex->owner != current_thread) {
        // 如果不是当前线程持有锁，直接返回（可选：输出错误信息）
        enable_interrupts();
        return;
    }
    
    mutex->locked = 0;
    mutex->owner = NULL;
    // 唤醒一个等待线程（如果有）
    thread_t *waiter = thread_dequeue_wait(mutex);
    if (waiter) {
        waiter->state = THREAD_READY;
        thread_enqueue(waiter);   // 放回就绪队列
    }
    enable_interrupts();
}

// 添加递归锁功能：检查当前线程是否拥有锁
int mutex_is_owned_by_current(mutex_t *mutex) {
    return mutex->owner == current_thread;
}

void KillThread(thread_t* thread) {
    if (!thread) return; //参数校验
    disable_interrupts(); //关中断，防止并发问题
    thread->state = THREAD_TERMINATED;//标记线程为终止状态
    remove_from_sleep_queue(thread);//从睡眠队列中移除（如果在线程中）
    //回收虚拟内存资源
    if(thread->use_vmm){
        VMM_T* vmm = thread->vmm;
        VMM_T* old_vmm = (VMM_T*)Switch_Virtual_Mamanager(vmm);
        if(vmm->AllocRecordNum > 0){
            for(int i = 0; i < vmm->AllocRecordNum; i++){
                Vmm_Free((void*)vmm->AllocRecord[i].Address, vmm->AllocRecord[i].Size);
            }
        }
        if(vmm->isUser){
            Pmm_Free(thread->kernel_stack, thread->stack_size);
        }else if((uint64_t)thread->kernel_stack >= KernelVirtualMemoryStart){
            Vmm_Free(thread->kernel_stack, thread->stack_size);
        }else{
            Pmm_Free(thread->kernel_stack, thread->stack_size / PAGE_SIZE);
        }
        thread->kernel_stack = NULL;
        Switch_Virtual_Mamanager(old_vmm);
    } else if (thread->kernel_stack) {
        Pmm_Free(thread->kernel_stack, thread->stack_size / PAGE_SIZE);
        thread->kernel_stack = NULL;
    }
    //如果是当前运行线程，则立即调度
    if (thread == current_thread) {
        //将 TCB 加入空闲链表
        if (num_of_FreeTCB < FREE_TCB_LIST_SIZE) {
            num_of_FreeTCB++;
            free_tcb_list[num_of_FreeTCB] = (uintptr_t)thread;
        }
        //清理 TORT 表中的条目
        TORT[thread->PositionInTable] = 0;
        enable_interrupts();//恢复中断
        schedule();//触发调度
        SYSTEM_STOP(); // 理论上不会执行到这里
    }
    //如果线程在就绪队列中，从队列中移除
    if (ready_queue_head == thread) {
        // 是队列头部
        ready_queue_head = thread->next;
        if (!ready_queue_head) ready_queue_tail = NULL;
    } else {
        // 遍历队列查找并移除
        thread_t* curr = ready_queue_head;
        while (curr && curr->next != thread) {
            curr = curr->next;
        }
        if (curr) {
            curr->next = thread->next;
            if (thread == ready_queue_tail) {
                ready_queue_tail = curr;
            }
        }
    }
    //将 TCB 加入空闲链表
    if (num_of_FreeTCB < FREE_TCB_LIST_SIZE) {
        num_of_FreeTCB++;
        free_tcb_list[num_of_FreeTCB] = (uintptr_t)thread;
    }
    TORT[thread->PositionInTable] = 0;//清理 TORT 表中的条目
    enable_interrupts(); //重新开启中断
}

// 睡眠指定毫秒数
void ThreadSleepMS(uint64_t ms) {
    if (ms == 0) return;
    disable_interrupts();
    // 计算唤醒时间（基于时钟滴答）
    current_thread->wakeup_time = SYSTEM_TimerTicks + (ms * PIT_CLOCK_FREQ) / 1000;
    current_thread->state = THREAD_BLOCKED;
    // 加入睡眠队列
    sleep_enqueue(current_thread);
    schedule(); // 触发调度
    enable_interrupts();
}

// 睡眠指定秒数
void ThreadSleepSecond(uint64_t s) {
    if (s == 0) return;
    disable_interrupts();
    // 计算唤醒时间
    current_thread->wakeup_time = SYSTEM_TimerTicks + (s * PIT_CLOCK_FREQ);
    current_thread->state = THREAD_BLOCKED;
    // 加入睡眠队列
    sleep_enqueue(current_thread);
    schedule(); // 触发调度
    enable_interrupts();
}

// 唤醒到达时间的睡眠线程
void wake_sleeping_threads(void) {
    // 保存当前中断状态，不直接 sti（可能在内核中断处理程序中被调用）
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags));
    disable_interrupts();
    thread_t *current = sleep_queue_head;
    thread_t *prev = NULL;
    while (current) {
        if (SYSTEM_TimerTicks >= current->wakeup_time) {
            // 时间到了，唤醒这个线程
            thread_t *to_wake = current;
            // 从睡眠队列中移除
            if (prev) {
                prev->next = current->next;
            } else {
                sleep_queue_head = current->next;
            }
            if (current == sleep_queue_tail) {
                sleep_queue_tail = prev;
            }
            current = current->next;
            // 重置连接指针
            to_wake->next = NULL;
            to_wake->state = THREAD_READY;
            // 加入就绪队列
            thread_enqueue(to_wake);
        } else {
            prev = current;
            current = current->next;
        }
    }
    // 恢复原中断状态而非直接 sti
    if (flags & 0x200) {
        enable_interrupts();
    }
}

//信号量相关函数
void sem_init(semaphore_t *sem, int value) {
    sem->count = value;
    sem->wait_queue_head = NULL;
    sem->wait_queue_tail = NULL;
}

static void sem_enqueue_wait(semaphore_t *sem, thread_t *thread) {
    thread->next = NULL;
    if (!sem->wait_queue_head) {
        sem->wait_queue_head = sem->wait_queue_tail = thread;
    } else {
        sem->wait_queue_tail->next = thread;
        sem->wait_queue_tail = thread;
    }
}

static thread_t *sem_dequeue_wait(semaphore_t *sem) {
    if (!sem->wait_queue_head) return NULL;
    thread_t *thread = sem->wait_queue_head;
    sem->wait_queue_head = thread->next;
    if (!sem->wait_queue_head) sem->wait_queue_tail = NULL;
    thread->next = NULL;
    return thread;
}

void sem_wait(semaphore_t *sem) {
    disable_interrupts();   // 关中断，保证原子性
    while (sem->count <= 0) {
        // 资源不足，当前线程进入等待队列并让出CPU
        current_thread->state = THREAD_BLOCKED;
        sem_enqueue_wait(sem, current_thread);
        schedule();//切换出去，返回时中断仍为关
    }
    // 成功获取资源
    sem->count--;
    enable_interrupts();
}

void sem_post(semaphore_t *sem) {
    disable_interrupts();
    sem->count++;  // 增加可用资源数
    // 唤醒一个等待线程
    thread_t *waiter = sem_dequeue_wait(sem);
    if (waiter) {
        waiter->state = THREAD_READY;
        thread_enqueue(waiter);//放回就绪队列
    }
    enable_interrupts();
}

void PrintRunningThreads(){
    printf("ID \tStatus \tStack Size   \tStack Address\n");
    printf("------------------------------------------------\n");
    printf("%d  \t%d      \t?KB   \t%p\n",current_thread->id,current_thread->state,current_thread->kernel_stack);
    //遍历线程表
    for(int i = 0; i < TORT_SIZE; i++){
        if(TORT[i] == 0)continue;//如果为0,跳过
        thread_t *thread = (thread_t*)TORT[i];
        printf("%d  \t%d      \t%dKB   \t%p\n",thread->id,thread->state,thread->stack_size/1024,thread->stack);
    }
}

void CleanAllThreads(){
    //遍历线程表
    for(int i = 0; i < TORT_SIZE; i++){
        if(TORT[i] == 0)continue;//如果无效,跳过
        thread_t *thread = (thread_t*)TORT[i];
        if((thread->id == 0) || (thread->id == 1))continue;//如果为主线程或IDLE，跳过
        KillThread(thread);
        out_ok("Thread ",0);
        printf("%d is over.\n",thread->id);
    }
}

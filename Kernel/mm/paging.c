#include <mm/paging.h>
#include <mm/vmm.h>
#include <Print.h>

void* malloc_page(){
    void* addr = Pmm_Malloc(1);

    memset(addr,0,PAGE_SIZE);
    return addr;
}

//复制当前PML4的前 `num_entries` 项到新页表
void copy_pml4_entries(pml4_t *dest, int num_entries) {
    pml4_t *src = (pml4_t*)UEFI_PML4;
    for (int i = 0; i < num_entries; i++) {
        (*dest)[i] = (*src)[i];
    }
}

// 只复制内核需要的 PML4 条目到用户 VMM 页表
// - PML4[0]：identity mapping（加 PTE_USER 允许用户态深入下级）
// - PML4[256-511]：内核高端空间（不加 PTE_USER，仅 Ring 0 访问）
// - PML4[1-255]：保留清零，供用户态分配
void copy_kernel_pml4_entries(pml4_t *dest) {
    pml4_t *src;
    if (Current_VMM_Desc && Current_VMM_Desc->Pml4) {
        src = Current_VMM_Desc->Pml4;  // 从当前内核 VMM 复制
    } else {
        src = UEFI_PML4;
    }

    // PML4[0]：identity mapping（内核代码数据在低地址）
    // 加 PTE_USER | PTE_WRITABLE 让用户态能访问下级页表
    // 但实际的 leaf page 没有 PTE_USER，用户仍不能访问内核页
    (*dest)[0] = (*src)[0] | PTE_USER | PTE_WRITABLE;

    // PML4[1-255]：保持清零 → 用户空间可用
    // （已经在 memset 清零过了，这里不用操作）

    // PML4[256-511]：内核高端空间，不加 PTE_USER，仅 Ring 0 可访问
    for (int i = 256; i < 512; i++) {
        (*dest)[i] = (*src)[i];
    }
}

// 根据虚拟地址在当前页表中遍历，如果 create 为真且中间表缺失，则自动分配
// 拆分大页：将 level 层的 huge entry 拆成 512 个子页
// level==2(PD): 2MB→4KB; level==1(PDPT): 1GB→2MB(子页仍 huge)
static uint64_t split_huge_page(pte_t *table, int level, int idx) {
    pte_t huge_entry = table[idx];
    uint64_t phys_base = huge_entry & PAGE_MASK;
    uint64_t base_flags = huge_entry & ~PAGE_MASK & ~PTE_HUGE;
    uint64_t sub_page_size;

    if (level == 2)
        sub_page_size = PAGE_SIZE;       // 2MB → 4KB
    else if (level == 1)
        sub_page_size = 0x200000;        // 1GB → 2MB
    else
        return 0;

    void *new_page = malloc_page();
    if (!new_page) return 0;
    pte_t *sub_table = (pte_t *)new_page;

    for (int i = 0; i < 512; i++) {
        uint64_t sub_phys = phys_base + i * sub_page_size;
        sub_table[i] = sub_phys | base_flags;
        if (level == 1)
            sub_table[i] |= PTE_HUGE;    // 2MB 子页
    }

    uint64_t sub_phys_addr = (uint64_t)new_page;
    table[idx] = sub_phys_addr | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    return sub_phys_addr;
}

static pte_t *walk_page_table(pml4_t *pml4, void *virt, int create) {
    uint64_t vaddr = (uint64_t)virt;
    uint64_t indices[4] = {
        PML4_INDEX(vaddr),
        PDPT_INDEX(vaddr),
        PD_INDEX(vaddr),
        PT_INDEX(vaddr)
    };
    pte_t *tables[4] = { (pte_t *)pml4, NULL, NULL, NULL };
    
    for (int level = 0; level < 4; level++) {
        pte_t *table = tables[level];
        pte_t entry = table[indices[level]];
        if (!(entry & PTE_PRESENT)) {
            if (!create) return NULL;
            void *new_page = malloc_page();
            if (!new_page) return NULL;
            uint64_t phys = (uint64_t)new_page;
            table[indices[level]] = phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
            entry = table[indices[level]];
        }
        // 非末级遇到大页 → 直接替换成空子页表，不拆分
        if ((level < 3) && (entry & PTE_HUGE)) {
            if (!create) return NULL;
            void *new_page = malloc_page();        // 全零页 = 全 NOT PRESENT
            if (!new_page) return NULL;
            uint64_t phys = (uint64_t)new_page;
            table[indices[level]] = phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
            entry = table[indices[level]];          // 重新读 entry
        }
        void *next_table = (void *)(entry & PAGE_MASK);
        if (level < 3) {
            tables[level + 1] = (pte_t *)next_table;
        } else {
            return &table[indices[level]];
        }
    }
    return NULL;
}

void unmap_page(void *virt) {
    uint64_t vaddr = (uint64_t)virt;
    pml4_t *pml4 = (pml4_t *)get_cr3();
    
    // 只遍历，不创建缺失的表
    pte_t *pt_entry = walk_page_table(pml4, virt, 0);
    if (!pt_entry) {
        // 映射不存在，直接返回
        return;
    }
    if (*pt_entry & PTE_PRESENT) {
        *pt_entry = 0;          // 清除页表项
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
}

// 映射一个虚拟地址到指定的物理地址
void map_page(void *virt, void *phys, uint64_t flags) {
    pml4_t *current_pml4 = (pml4_t *)get_cr3();
    pte_t *pt_entry = walk_page_table(current_pml4, virt, 1);
    if (!pt_entry) {
        out_error("[map_page]");
        printf("walk failed for %p\n", virt);
        return;
    }
    #ifdef PAGING_DEBUG
    if (*pt_entry & PTE_PRESENT) {
        out_ok("[map_page]",0);
        printf(" %p already mapped, overwriting\n", virt);
    }
    #endif
    *pt_entry = (uint64_t)phys | flags | PTE_PRESENT;
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");//刷新 TLB
}

void init_paging(void) {
    pml4_t *new_pml4 = (pml4_t*)malloc_page();//分配一个新 PML4 页
    if (!new_pml4) {
        out_error("Failed to allocate new PML4\n");
        return;
    }
    copy_pml4_entries(new_pml4, 512);//复制当前 PML4 的所有条目（保持原有映射不变）
    uint64_t new_cr3 = (uint64_t)new_pml4;//切换到新页表
    set_cr3(new_cr3);
    out_ok("Successfully switched to new page table (copy of UEFI's).\n", 0);
}

/*Deepseek*/

uintptr_t virt_to_phys_on(pml4_t *pml4, void *virt) {
    uint64_t vaddr = (uint64_t)virt;
    uint64_t offset = vaddr & (PAGE_SIZE - 1);   // 页内偏移
    
    // 1. PML4 层
    uint64_t pml4_idx = PML4_INDEX(vaddr);
    pte_t pml4e = (*pml4)[pml4_idx];
    if (!(pml4e & PTE_PRESENT)) return 0;
    if (pml4e & PTE_HUGE) {
        // 理论上 PML4 项不可能为 huge，但以防万一
        uint64_t phys_base = pml4e & PAGE_MASK;
        return phys_base + (vaddr & ((1ULL << 39) - 1));
    }
    
    // 2. PDPT 层
    pte_t *pdpt = (pte_t*)(pml4e & PAGE_MASK);
    uint64_t pdpt_idx = PDPT_INDEX(vaddr);
    pte_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & PTE_PRESENT)) return 0;
    if (pdpte & PTE_HUGE) {
        // 1GB 大页
        uint64_t phys_base = pdpte & PAGE_MASK;
        return phys_base + (vaddr & ((1ULL << 30) - 1));
    }
    
    // 3. PD 层
    pte_t *pd = (pte_t*)(pdpte & PAGE_MASK);
    uint64_t pd_idx = PD_INDEX(vaddr);
    pte_t pde = pd[pd_idx];
    if (!(pde & PTE_PRESENT)) return 0;
    if (pde & PTE_HUGE) {
        // 2MB 大页
        uint64_t phys_base = pde & PAGE_MASK;
        return phys_base + (vaddr & ((1ULL << 21) - 1));
    }
    
    // 4. PT 层（4KB 页）
    pte_t *pt = (pte_t*)(pde & PAGE_MASK);
    uint64_t pt_idx = PT_INDEX(vaddr);
    pte_t pte = pt[pt_idx];
    if (!(pte & PTE_PRESENT)) return 0;
    
    uint64_t phys_base = pte & PAGE_MASK;
    return phys_base + offset;
}

// 在当前 CR3 页表中查询
uintptr_t virt_to_phys(void *virt) {
    pml4_t *current_pml4 = (pml4_t*)get_cr3();
    return virt_to_phys_on(current_pml4, virt);
}

// 使物理页在 identity mapping 中可写
// Pmm_Malloc 可能返回 UEFI/OVMF 标记为只读的页（如 BootServicesData/Code 区域），
// walk_page_table 通过 identity mapping 访问这些页时写操作会触发 #PF
void make_phys_writable(void *phys) {
    // 手动遍历页表，在每一层加 PTE_WRITABLE
    // 不拆分 huge page（直接加 RW 位），避免递归分配页表页
    pml4_t *pml4 = (pml4_t *)get_cr3();
    uint64_t vaddr = (uint64_t)phys;

    // Level 0: PML4
    pte_t pml4e = (*pml4)[PML4_INDEX(vaddr)];
    if (!(pml4e & PTE_PRESENT)) return;
    if (!(pml4e & PTE_WRITABLE))
        (*pml4)[PML4_INDEX(vaddr)] |= PTE_WRITABLE;

    // Level 1: PDPT
    pte_t *pdpt = (pte_t*)(pml4e & PAGE_MASK);
    pte_t pdpte = pdpt[PDPT_INDEX(vaddr)];
    if (!(pdpte & PTE_PRESENT)) return;
    if (pdpte & PTE_HUGE) {
        // 1GB huge page → 直接加 RW，不拆分
        pdpt[PDPT_INDEX(vaddr)] |= PTE_WRITABLE;
        asm volatile("invlpg (%0)" : : "r"(phys) : "memory");
        return;
    }
    if (!(pdpte & PTE_WRITABLE))
        pdpt[PDPT_INDEX(vaddr)] |= PTE_WRITABLE;

    // Level 2: PD
    pte_t *pd = (pte_t*)(pdpte & PAGE_MASK);
    pte_t pde = pd[PD_INDEX(vaddr)];
    if (!(pde & PTE_PRESENT)) return;
    if (pde & PTE_HUGE) {
        // 2MB huge page → 直接加 RW
        pd[PD_INDEX(vaddr)] |= PTE_WRITABLE;
        asm volatile("invlpg (%0)" : : "r"(phys) : "memory");
        return;
    }
    if (!(pde & PTE_WRITABLE))
        pd[PD_INDEX(vaddr)] |= PTE_WRITABLE;

    // Level 3: PT（叶子页表）
    pte_t *pt = (pte_t*)(pde & PAGE_MASK);
    pte_t pte = pt[PT_INDEX(vaddr)];
    if (!(pte & PTE_PRESENT)) return;
    pt[PT_INDEX(vaddr)] |= PTE_WRITABLE;
    asm volatile("invlpg (%0)" : : "r"(phys) : "memory");
}

void replace_identity_pdpt(pml4_t *pml4) {
    pte_t pml4e = (*pml4)[0];
    if (!(pml4e & PTE_PRESENT)) return;

    pte_t *old_pdpt = (pte_t *)(pml4e & PAGE_MASK);
    pte_t *new_pdpt = (pte_t *)malloc_page();

    for (int i = 0; i < 512; i++) {
        if (old_pdpt[i] & PTE_PRESENT) {
            if (old_pdpt[i] & PTE_HUGE) {
                // 本来就是 1GB huge page → 直接加 W
                new_pdpt[i] = old_pdpt[i] | PTE_WRITABLE;
            } else {
                // UEFI 用了 4KB/2MB 细粒度子页表（可能有只读 entry）
                // → 替换成 1GB huge page + W，绕过下层权限限制
                uint64_t phys_base = (uint64_t)i << 30;  // i * 1GB
                new_pdpt[i] = phys_base | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE | PTE_ACCESSED;
            }
        }
    }

    (*pml4)[0] = (uint64_t)new_pdpt | (pml4e & ~PAGE_MASK) | PTE_WRITABLE;
}
/*Deepseek END*/
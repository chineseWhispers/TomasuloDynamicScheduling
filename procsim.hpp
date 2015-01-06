#ifndef PROCSIM_HPP
#define PROCSIM_HPP

#include <cstdint>
#include <cstdio>

#define DEFAULT_K0 1
#define DEFAULT_K1 2
#define DEFAULT_K2 3
#define DEFAULT_R 8
#define DEFAULT_F 4

typedef struct procInfo{
    uint64_t f;
    uint64_t r;
    uint64_t k0;
    uint64_t k1;
    uint64_t k2;
    uint64_t scheQSize;
}procInfo;

typedef struct regFile{
    int32_t ready;
    int32_t tag;
    int32_t value;
}regFile;

typedef struct CDB{
    int32_t busy;
    int32_t ready;
    int32_t tag;
    int32_t value;
    int32_t reg;
}CDB;

typedef struct _proc_inst_t
{
    uint32_t instNum;
    uint32_t instruction_address;
    int32_t op_code;
    int32_t src_reg[2];
    int32_t dest_reg;
    int32_t exPriority;
    int32_t dispatchFlag; // for dispatching or ready to dispatch
    int32_t scheduleFlag; // for scheduling or ready to schedule
    int32_t issueFlag; // for executing or ready to issue
    int32_t suFlag; // for state updating or ready to retir
    int32_t eraseFlag; 
    int32_t IF;
    int32_t DISP;
    int32_t SCH;
    int32_t EX;
    int32_t SU;
    // You may introduce other fields as needed
} proc_inst_t;

typedef struct scheQEntry{
    int32_t instNum;
    int32_t op_code; // FU type
    int32_t src_reg_name[2];
    int32_t src_reg_value[2];
    int32_t src_reg_tag[2];
    int32_t src_reg_ready[2];
    int32_t dest_reg;
    int32_t dest_reg_tag;
    
}scheQEntry;

typedef struct _proc_stats_t
{
    float avg_inst_retired;
    float avg_inst_fired;
    float avg_disp_size;
    unsigned long max_disp_size;
    unsigned long retired_instruction;
    unsigned long cycle_count;
} proc_stats_t;

bool read_instruction(proc_inst_t* p_inst);

void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f);
void run_proc(proc_stats_t* p_stats);
void complete_proc(proc_stats_t* p_stats);

#endif /* PROCSIM_HPP */

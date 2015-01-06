//Yingyan Wang

#include "procsim.hpp"
#include <iostream>
#include <cstdio>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

using namespace std;

vector <proc_inst_t> dispatchQ;
vector <proc_inst_t> scheduleQ;
vector <proc_inst_t> suQ;
vector <scheQEntry> RS;
string output[100001];
regFile RF[50];
int scoreboard[3]; //FU unit
CDB* cdb;
procInfo* tomasulo;
int uniqueTag=1;
int dispatchSig = 0;
int schedSig = 0;
int issueSig = 0;
int suSig = 0;
int instNum = 0;
int cdbbusy = 0;
int initFlag = 0;
int cycle = 1;
int totalInst = 0;
int maxDispQ = 0;
double sum_dispQ = 0;
float avg_dispQ = 0;
double sum_fire = 0;
float avg_fire = 0;
double sum_retire = 0;
float avg_retire = 0;

/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) 
{
    tomasulo = (procInfo*)malloc(sizeof(procInfo));
    tomasulo -> f = f;
    tomasulo -> r = r;
    tomasulo -> k0 = k0;
    tomasulo -> k1 = k1;
    tomasulo -> k2 = k2;
    tomasulo -> scheQSize = 2*(k0+k1+k2);
    cdb = (CDB*)malloc(r*sizeof(CDB));
    for (int i=0;i<50;i++){
        RF[i].ready = 1;
        RF[i].tag = -1;
        RF[i].value = 10; //arbitrary initialized
    } 
    for (int j=0;j<(int)r;j++){
        cdb[j].busy =0;
        cdb[j].ready=0;
        cdb[j].tag = -1;
        cdb[j].value = -1;
    }
    for (int k = 0; k<3;k++){
        scoreboard[k]=0; //busy
    }
}

//Fetching
void fetch(){
    proc_inst_t tmp;
    memset(&tmp, 0, sizeof(proc_inst_t));        
    if (read_instruction(&tmp)){
        tmp.dispatchFlag = 1; // flag as ready to dispatch next cycle
        tmp.instNum = ++instNum;
        tmp.IF = cycle;
        tmp.DISP = 0;
        tmp.SCH = 0;
        tmp.EX = 0;
        tmp.SU = 0;
        dispatchQ.push_back(tmp);         
        dispatchSig = 1; 
    }
}

//Dispatching Unit
void dispatch(){
  scheQEntry tmp;
  memset(&tmp, 0, sizeof(scheQEntry));
  for (int i=0;i<(int)dispatchQ.size();i++){
    if (dispatchQ[i].dispatchFlag == 1 && dispatchQ[i].DISP ==0){
      dispatchQ[i].DISP = cycle;
    }
  } 
  while ((int)dispatchQ.size()>0){
    if ((int)dispatchQ.size() > maxDispQ){
      maxDispQ = (int)dispatchQ.size();
    }
    if ((int)scheduleQ.size()<(int)(tomasulo->scheQSize)){
      dispatchSig = 0;
      dispatchQ[0].scheduleFlag = 1; 
      schedSig = 1;
      scheduleQ.push_back(dispatchQ[0]);
      dispatchQ.erase(dispatchQ.begin());
      tmp.instNum = scheduleQ.back().instNum;
      tmp.op_code = scheduleQ.back().op_code; //FU
      if (scheduleQ.back().dest_reg == -1){
        tmp.dest_reg = 49;
      }
      else{
        tmp.dest_reg = scheduleQ.back().dest_reg;
      }
      for (int j=0;j<=1;j++){
        if (scheduleQ.back().src_reg[j]==-1){          
          tmp.src_reg_ready[j] = 1; //no reg
        }
        else if (RF[scheduleQ.back().src_reg[j]].ready == 1){
          tmp.src_reg_value[j] = RF[scheduleQ.back().src_reg[j]].value;
          tmp.src_reg_tag[j] = RF[scheduleQ.back().src_reg[j]].tag;
          tmp.src_reg_ready[j] = 1;
        }
        else{
          tmp.src_reg_tag[j] = RF[scheduleQ.back().src_reg[j]].tag;
          tmp.src_reg_ready[j] = 0;
        }
      }
      RF[tmp.dest_reg].tag = uniqueTag;
      tmp.dest_reg_tag = uniqueTag;
      RF[tmp.dest_reg].ready = 0;  
      uniqueTag++;
      RS.push_back(tmp);
    }
    else{
      dispatchSig = 1;      
      break; // wait for next cycle to dispatch, dispatch flag remains 1
    }
  }
}

bool isFUBusy(int index){
  bool isBusy = false;
  if (index == -1){
    index = 1; // opcode 1 and -1 executed in the same FU
  }
  if (index ==0){
    isBusy = scoreboard[index]>=((int)tomasulo->k0);
  }
  if (index ==1){
    isBusy = scoreboard[index]>=((int)tomasulo->k1);
  }
  if (index ==2){
    isBusy = scoreboard[index]>=((int)tomasulo->k2);
  }
  return isBusy;
}


void schedule(){
  int localSched = 0;
  for (int i=0;i<(int)RS.size();i++){
    if (scheduleQ[i].scheduleFlag == 1 && scheduleQ[i].SCH ==0){
      scheduleQ[i].SCH = cycle;
    }
  }
  for (int k = 0; k<(int)RS.size();k++){ // for each entry in RS
    if (scheduleQ[k].scheduleFlag == 1){
      for (int i=0;i<=1;i++){ // for each source register
        if (RS[k].src_reg_ready[i]==0){
          for (int j=0;j<(int)tomasulo->r;j++){ // for each cdb
            if (cdb[j].tag == RS[k].src_reg_tag[i]){
              RS[k].src_reg_ready[i]=1;
              RS[k].src_reg_value[i]=cdb[j].value;
              break;
            }
          }       
        }
      }  
      if (RS[k].src_reg_ready[0]==1 && RS[k].src_reg_ready[1]==1
         && !isFUBusy(RS[k].op_code)){  
        if (RS[k].op_code == -1){
          scoreboard[1]++;
        }
        else{
          scoreboard[RS[k].op_code]++;
        }
        issueSig = 1;
        scheduleQ[k].issueFlag=1; // flag as ready to issue
        scheduleQ[k].scheduleFlag=0;
        schedSig = 0;
        sum_fire++;
      }
      else{
        localSched = 1;
      }
    }
  }
  schedSig = localSched;
}

int findMaxPrior(){
  int max = 0;
  for (int i=0;i<(int)RS.size();i++){
    if (scheduleQ[i].exPriority > max){
      max = scheduleQ[i].exPriority;
    }
  }
  return max;
}

//Executing Unit - all three has latency of 1
void execute(){
  int exeNum = 0;
  for (int x = 0;x<(int)RS.size();x++){
    if (scheduleQ[x].issueFlag == 1 && scheduleQ[x].EX==0){
      scheduleQ[x].EX=cycle;
    }
  } 

  for (int i = 0;i<(int)RS.size();i++){
    if (scheduleQ[i].issueFlag == 1){
      scheduleQ[i].exPriority++;
      exeNum++;
    }
  }
  int maxPrior = findMaxPrior();
  int newMaxPrior= maxPrior;
  int i = 0;
  while(i<(int)RS.size()){
    if (scheduleQ[i].issueFlag == 1 && scheduleQ[i].exPriority == maxPrior){
      for (int k=0;k<(int)tomasulo->r;k++){
        if (cdb[k].busy ==0){
          cdb[k].busy = 1;
          scheduleQ[i].suFlag = 1; //ready move on to state update 
          scheduleQ[i].issueFlag = 0;
          scheduleQ[i].exPriority=0;
          newMaxPrior = findMaxPrior();
          suSig = 1;
          cdbbusy++;
          if (RS[i].op_code ==-1){
            scoreboard[1]--;
          }
          else{
            scoreboard[RS[i].op_code]--;
          }
          break;
        }
      }
      if (cdbbusy == (int)tomasulo->r ){ 
        break;
      }
    }
    if (newMaxPrior<maxPrior){
        maxPrior = newMaxPrior;
        i=0;
    }
    else{
      i++;
    }
  }
  if (exeNum > cdbbusy){
    issueSig = 1;
  }
  else{
    issueSig = 0;
  }
}


void stateUpdate(){
  int k = 0;
  for (int i=0;i<(int)RS.size();i++){ 
    if (scheduleQ[i].suFlag ==1 && scheduleQ[i].SU==0){  
      scheduleQ[i].SU = cycle; 
    }
  }

  for (int i=0;i<(int)RS.size();i++){ 
    if (scheduleQ[i].suFlag ==1){   
      cdb[k].tag = RS[i].dest_reg_tag;
      cdb[k].reg = RS[i].dest_reg;
      scheduleQ[i].eraseFlag = 1;
      char buf[30];
      sprintf(buf,"%d\t%d\t%d\t%d\t%d\t%d",scheduleQ[i].instNum,scheduleQ[i].IF,scheduleQ[i].DISP,scheduleQ[i].SCH,scheduleQ[i].EX,scheduleQ[i].SU);
      output[scheduleQ[i].instNum] = buf;
      k++;
      if (k >= (int)tomasulo->r){
        break;
      }
    }
  }
  for (int k = 0;k<(int)tomasulo->r;k++){
    if (cdb[k].tag!=-1 && cdb[k].busy ==1 && cdb[k].tag == RF[cdb     [k].reg].tag){
           RF[cdb[k].reg].ready = 1;
           RF[cdb[k].reg].value = cdb[k].value;
    }
    cdb[k].busy = 0;
    cdbbusy--;
  }
}

void stateUpdate2(){
  for (int i=0;i<(int)RS.size();i++){
    if (scheduleQ[i].eraseFlag ==1){
      totalInst++;
      RS.erase(RS.begin()+i);
      sum_retire++;
      scheduleQ.erase(scheduleQ.begin()+i);
      i--;
    }
  }
}


void printDispatchQ(){
    for(int x = 0; x<(int)dispatchQ.size(); x++) 
    {
        cout<<dispatchQ[x].instruction_address<<" ";   
        cout<<dispatchQ[x].op_code<<" ";
        cout<<dispatchQ[x].dest_reg<<" ";
        cout<<dispatchQ[x].src_reg[0]<<" ";   
        cout<<dispatchQ[x].src_reg[1]<<endl;    
    }
    cout << "---------------" << endl; 
}

void printOutput(){
  char title[30];
  sprintf(title, "INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE");
  output[0] = title;
  for (int i=0;i< totalInst+1;i++){
    cout << output[i]<<endl;
  }
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats)
{
  while(scheduleQ.size()>0 || cycle<3){
    if (suSig ==1){
      stateUpdate();
    }
    if (issueSig ==1){
      execute();
    }
    if (schedSig == 1){
      schedule();
    }
    if (dispatchSig == 1 ){
      dispatch();
    }
    stateUpdate2();
    for (int i=0;i<(int)(tomasulo->f);i++){
      fetch();
    }
    cycle ++;
    sum_dispQ = sum_dispQ+dispatchQ.size();
  }
  printOutput();
  cout <<""<<endl;
  avg_dispQ = (double)sum_dispQ/(float)(cycle-1);
  avg_fire = (double)sum_fire/(float)(cycle-1);
  avg_retire = (double)sum_retire/(float)(cycle-1);
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) 
{
  p_stats->retired_instruction = totalInst;
  p_stats-> max_disp_size = maxDispQ;
  p_stats->avg_disp_size = avg_dispQ;
  p_stats->avg_inst_fired = avg_fire;
  p_stats->avg_inst_retired = avg_retire;
  p_stats->cycle_count = cycle-1;
}

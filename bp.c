/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <math.h>
//#include <stdbool.h> // api 


// FSM states
#define SNT 0
#define WNT 1
#define WT 2
#define ST 3

#define NOT_USING_SHARE 0
#define USING_SHARE_LSB 1
#define USING_SHARE_MID 2

uint32_t pc_to_btb_entry (uint32_t pc , unsigned len, unsigned shift);

//single btb entry 
typedef struct Btb_entry{
	uint32_t tag;
	uint32_t target;
	uint32_t localhistory;
	int *fsmLocal; //local fsm 
}Btb_entry;



unsigned btbSize_p;
unsigned historySize_p;
unsigned tagSize_p;
unsigned fsmState_p;
bool isGlobalHist_p;
bool isglobalTable_p;
int shared_p;

int *fsmGlobal; //global fsm
Btb_entry *btbTable; // btb table
uint32_t globalHistory_p;


int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){

	//fsm global init
	fsmGlobal= (int*)malloc((1 << historySize)*sizeof(int));// int(pow(n,2))<--> 1 << btbSize
if (!(fsmGlobal)){
	return -1;//cheak malloc...
} 
for(unsigned j=0; j< (1 << historySize); j++){
	fsmGlobal[j]=fsmState;
}
	btbTable=(Btb_entry * ) malloc(btbSize * sizeof(Btb_entry));
	if (!(btbTable)){
		//cleanup
		free
		return -1;//cheak malloc...
	} 
	for(unsigned i =0; i< btbSize ;i++){
		btbTable[i].fsmLocal= (int*)malloc((1 << historySize) *sizeof(int));// pow(n,2) <--> 1 << btbSize
		if (!(btbTable[i].fsmLocal)){
			
			// Cleanup 
			for (unsigned k = 0; k < i; k++) {
				free(btbTable[k].fsmLocal);
			}
			free(btbTable);
			return -1;
		} 

		for(unsigned j=0; j< (1 << historySize); j++){
			btbTable[i].fsmLocal[j]=fsmState;
		}
		btbTable[i].tag=0;
		btbTable[i].target=0;
		btbTable[i].localhistory=0;
	
	}

	
	//param init 
	globalHistory_p=0;
	btbSize_p=btbSize;
	historySize_p = historySize;
	tagSize_p = tagSize;
    fsmState_p = fsmState;
    isGlobalHist_p = isGlobalHist;
	isglobalTable_p  = isGlobalTable;
	shared_p = shared;

	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
	int btbEnt=pc_to_btb_entry(pc,btbSize_p,2);
	int
	unsigned fsmVal;
	if(!isGlobalHist_p){
		if(!isglobalTable_p){
			if(btbTable[btbEnt].tag == pc_to_btb_entry(pc,tagSize_p,2)){
				fsmVal = btbTable[btbEnt].fsmLocal[btbTable[btbEnt].localhistory];
				if(fsmVal >= 2){
					*dst=btbTable[btbEnt].target;
					return true;
				}
	
			}
		}
		else{
			if(btbTable[btbEnt].tag == pc_to_btb_entry(pc,tagSize_p,2)){
				uint32_t fsmTableEntry;
				switch(shared_p){
					case(NOT_USING_SHARE):
						fsmTableEntry=btbTable[btbEnt].local_history;
						break;
					case(USING_SHARE_LSB):
						fsmTableEntry=(btbTable[btbEnt].local_history) ^ (pc_to_btb_entry(pc,historySize_p,2));
						break;
					case(USING_SHARE_MID):
						fsmTableEntry=(btbTable[btbEnt].local_history) ^ (pc_to_btb_entry(pc,historySize_p,16));
						break;

				}
				fsmVal = fsmGlobal[fsmTableEntry];
				if(fsmVal >= 2){
					*dst=btbTable[btbEnt].target;
					return true;
				}
			
		
			}

		}
	}
	else{
		uint32_t fsmTableEntry;
				switch(shared_p){
					case(NOT_USING_SHARE):
						fsmTableEntry=globalHistory_p;
						break;
					case(USING_SHARE_LSB):
						fsmTableEntry=(globalHistory_p) ^ (pc_to_btb_entry(pc,historySize_p,2));
						break;
					case(USING_SHARE_MID):
						fsmTableEntry=(globalHistory_p) ^ (pc_to_btb_entry(pc,historySize_p,16));
						break;

				}
		fsmVal=fsmGlobal[fsmTableEntry];
		if(fsmVal >= 2){
			*dst=btbTable[btbEnt].target;
			return true;
		}

	}
	*dest = pc + 4; 
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	return;
}

void BP_GetStats(SIM_stats *curStats){
	return;
}
/////////////////////////
uint32_t pc_to_btb_entry (uint32_t pc , unsigned len, unsigned shift){
	uint32_t mask = (1<<(int)log2(len))-1;
	uint32_t btbEntry= (pc >> shift ) ;
	btbEntry &= mask;
	return btbEntry;
}


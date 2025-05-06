/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */
/* C file */
/* update 05/05/2025 - */
#include "bp_api.h"
#include <stdlib.h>


// share define
#define NOT_USING_SHARE 0 // not using share
#define USING_SHARE_LSB 1  // using lsb share 
#define USING_SHARE_MID 2  // using  mid share


//HELP FUNCTION DECLERATION:
uint32_t bit_slice (uint32_t field , unsigned len , unsigned shift);


// FSM states -using enum
enum FsmState {
	SNT=0, // strongly not taken
	WNT=1, // weakly not taken
	WT=2, // weakly taken
	ST=3 // strongly taken
};

// BTB entry  struct
typedef struct BtbEntry{
	uint32_t tag; 
	uint32_t target;
	uint32_t localHistory;
	int *localFsm; // pointer to local fsm 
	bool validBit;
}BtbEntry;

// BTB global configuration :

static unsigned bt_btbSize;
static unsigned bt_historySize;
static unsigned bt_tagSize;
static unsigned bt_fsmState;
static bool bt_isGlobalHist;
static bool bt_isGlobalTable;
static int bt_shared;
static uint32_t bt_globalHistory;

static int *bt_globalFsm=NULL;
static BtbEntry *bt_btbTable= NULL;

// statistics tracking

static unsigned numberOfPredictions =0; // number of predictions
static unsigned numberOfFlushes =0 ; // number of flushes





/* initialization function :*/
int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){

	/*----set btb configuration -----*/
	bt_btbSize=btbSize;
	bt_historySize = historySize;
	bt_tagSize = tagSize;
	bt_fsmState = fsmState;
	bt_isGlobalHist = isGlobalHist;
	bt_isGlobalTable = isGlobalTable;
	bt_shared = Shared;
	bt_globalHistory=0;

	
	// allocate global fsm if needed :
	if(bt_isGlobalTable){
		bt_globalFsm = (int*)malloc(sizeof(int)*(1 << bt_historySize));
		if(!bt_globalFsm){
			return -1;
		}
		for(unsigned i =0; i<(1 << bt_historySize); i++){
			bt_globalFsm[i]=bt_fsmState;
		}
	}
	

	//allocate btb table
	bt_btbTable = (BtbEntry*)malloc(sizeof(BtbEntry)*btbSize);
	if(!bt_btbTable){
		if(bt_isGlobalTable){
			free(bt_globalFsm);
		}
		return -1;
	}

	for(unsigned i =0 ; i< bt_btbSize ;i++){
		bt_btbTable[i].tag=0;
		bt_btbTable[i].target=0;
		bt_btbTable[i].localHistory=0;
		bt_btbTable[i].validBit=false;

		// allocate local fsm if needed
		if(!bt_isGlobalTable){
			bt_btbTable[i].localFsm=(int*)malloc(sizeof(int)*(1 << bt_historySize));
			if(!bt_btbTable[i].localFsm){
				for(unsigned k =0 ; k< i ;k++){
					free(bt_btbTable[k].localFsm);
				}
				free(bt_btbTable);
				return -1;
			}

			for(unsigned j =0 ; j<(1 << bt_historySize) ; j++){
				bt_btbTable[i].localFsm[j]=bt_fsmState;
			}

		}
		else{
			bt_btbTable[i].localFsm=NULL;
		}
	}

	return 0; // success

}


/* prediction function */
bool BP_predict(uint32_t pc, uint32_t *dst){

	// calc index and tag
	unsigned btbIndexBits = __builtin_ctz(bt_btbSize);
	uint32_t index = bit_slice(pc, btbIndexBits, 2);
	uint32_t tag = bit_slice(pc, bt_tagSize , 2 + btbIndexBits );
	//update number of predictions
	numberOfPredictions++;

	//check 
	if (!bt_btbTable[index].validBit || bt_btbTable[index].tag != tag) {
		*dst = pc + 4;
		return false;
	}
	
	
	//  get history global/local
	uint32_t historySource = bt_isGlobalHist ? bt_globalHistory : bt_btbTable[index].localHistory;

	//  apply share 
	if(bt_isGlobalTable){
		if(bt_shared == USING_SHARE_LSB){
			historySource ^= bit_slice(pc , bt_historySize , 2);
		}

		if(bt_shared == USING_SHARE_MID){
			historySource ^= bit_slice(pc , bt_historySize , 16);
		}
	}

	// fsm index calc
	 uint32_t fsmIndex = historySource & ((1 << bt_historySize) -1);
	 // get current state
	 int currentState = bt_isGlobalTable ? bt_globalFsm[fsmIndex] : bt_btbTable[index].localFsm[fsmIndex];

	 bool taken = (currentState >= WT);
	 *dst= taken ? bt_btbTable[index].target : pc+4;
	 return taken; 
}



/* bp update function */
void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	
	// update statistics :
	if((taken && targetPc != pred_dst) || (!taken && ((pc+4) != pred_dst))) {
		numberOfFlushes++;
	}
	// calc index and tag
	unsigned btbIndexBits = __builtin_ctz(bt_btbSize);
	uint32_t index = bit_slice(pc, btbIndexBits, 2);
	uint32_t tag = bit_slice(pc, bt_tagSize , 2 + btbIndexBits );

	//update entry if neeeded
	if(!bt_btbTable[index].validBit || bt_btbTable[index].tag != tag){
		bt_btbTable[index].tag=tag;
		bt_btbTable[index].localHistory=0;
		bt_btbTable[index].target= targetPc;
		bt_btbTable[index].validBit = true;
		if(!bt_isGlobalTable){
			for(unsigned j =0 ; j<(1 << bt_historySize) ; j++){
				bt_btbTable[index].localFsm[j]=bt_fsmState;
			}
		}

	}

	// get history global/local
	uint32_t historySource = bt_isGlobalHist ? bt_globalHistory : bt_btbTable[index].localHistory;

	//  apply share 
	if(bt_isGlobalTable){
		if(bt_shared == USING_SHARE_LSB){
			historySource ^= bit_slice(pc , bt_historySize , 2);
		}

		if(bt_shared == USING_SHARE_MID){
			historySource ^= bit_slice(pc , bt_historySize , 16);
		}
	}

	// fsm index calc
	uint32_t fsmIndex = historySource & ((1 << bt_historySize) -1);

	// update fsm (global or local ) and update history (global or local)
	if(bt_isGlobalTable){
		if(taken && bt_globalFsm[fsmIndex] < ST){
			bt_globalFsm[fsmIndex]++;
		}
		else if(!taken && bt_globalFsm[fsmIndex] > SNT){
			bt_globalFsm[fsmIndex]--;
		}
	} else {
		if(taken && bt_btbTable[index].localFsm[fsmIndex] < ST){
			bt_btbTable[index].localFsm[fsmIndex]++;
		}
		else if(!taken && bt_btbTable[index].localFsm[fsmIndex] > SNT){
			bt_btbTable[index].localFsm[fsmIndex]--;
		}
	}

	if(bt_isGlobalHist){
		bt_globalHistory = ((bt_globalHistory << 1) | taken ) & ((1 << bt_historySize) -1);
	}
	else{
		bt_btbTable[index].localHistory = ((bt_btbTable[index].localHistory << 1) | taken ) & ((1 << bt_historySize) -1);
	}
	//update target
	bt_btbTable[index].target = targetPc;

	return;
}
 // clean up and return statistics
void BP_GetStats(SIM_stats *curStats){

	curStats->flush_num =numberOfFlushes;
	curStats->br_num = numberOfPredictions;

	//memory usage calc - in theory
	unsigned memorySize = 0;
	if(bt_isGlobalHist){
		memorySize +=bt_historySize;
	}
	else{
		memorySize += bt_historySize *bt_btbSize;
	}

	if(bt_isGlobalTable){
		memorySize += 2* (1 << bt_historySize);
	}
	else{
		memorySize += bt_btbSize * 2 * (1 << bt_historySize);
	}
	memorySize += bt_btbSize * (bt_tagSize+30+ 1);
	curStats->size = memorySize;


	//cleanUp :) - free all allocated memory

	if(bt_isGlobalTable){
		free(bt_globalFsm);
	}
	else{
		for(unsigned i =0 ; i< bt_btbSize ;i++){
			free(bt_btbTable[i].localFsm);
		}

	}
	free(bt_btbTable);
	return;
}

/* bit_slicing help function */
uint32_t bit_slice (uint32_t field , unsigned len , unsigned shift){

	uint32_t  mask = (len == 32) ? 0xFFFFFFFF : ((1 << len) - 1); //mask
	return (field >> shift) & mask ;
}


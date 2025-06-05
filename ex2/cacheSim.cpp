// last update 5/6/2025 - 20:00
//course - Computer Architecture 046267
//hw-2 cache simulator :(
// id : 213306194 , id: 205663776
/*------------------------------------------------*/
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;
using namespace std;
#define ADDRESS_SIZE 32 //address is 32 bit long
/*--------------------------------------------------------------------------------------------------------*/
 //define struct cache block
struct cacheBlock {
    unsigned tag = 0; // tag
    bool valid = 0; // valid bit
    bool dirty = 0; // dirty bit 
    unsigned evictCount = 0; // evicted counter lru
};

// class cache level 
class Cache {
private:
    unsigned cacheSize; // size
    unsigned blockSize, assoc; // block size ans assoc
    unsigned accessTime; // access time 
    unsigned num_of_sets; //number of sets
    unsigned num_of_ways; //number of ways
    unsigned set_size; // set size in bits
    unsigned offset_size; //offset size in bits
    unsigned tag_size; // tag size in bits
    vector<vector<cacheBlock>> blocks; // vector of vector of blocks

public:
// consrtuctor
    Cache(unsigned cacheSize, unsigned blockSize, unsigned assoc, unsigned accessTime)
        : cacheSize(cacheSize), blockSize(blockSize), assoc(assoc), accessTime(accessTime) {
        set_size = cacheSize - blockSize - assoc; // calc of set size - can be shown log2 of num of sets
        num_of_sets = 1 << set_size; // pow2
        num_of_ways = 1 << assoc; // pow2
        offset_size = blockSize; // in bits- log2(bytes)
        tag_size = ADDRESS_SIZE - offset_size - set_size; // calc    
        blocks.resize(num_of_sets, vector<cacheBlock>(num_of_ways)); //resize vector of vectors
    }

    // Check for hit
    bool checkHit(unsigned address, unsigned& way_index) {
        unsigned index = (address >> offset_size) & ((1 << set_size) - 1); // calc index
        unsigned tag = address >> (offset_size + set_size); // calc tag
         // look for hit    
        for (unsigned i = 0; i < num_of_ways; i++) {
            if (blocks[index][i].valid && blocks[index][i].tag == tag) {
                way_index = i;
                return true;
            }
        }
        return false;
    }
    // Find invalid way- empty block
    bool findInvalidWay(unsigned index, unsigned& way_index) {
        for (unsigned i = 0; i < num_of_ways; i++) {
            if (!blocks[index][i].valid) {
                way_index = i;
                return true;
            }
        }
        return false;
    }
    // find oldest - by lru alg
    void findByEvictedCount(unsigned index, unsigned& way_index) {
        for (unsigned i = 0; i < num_of_ways; i++) {
            if (blocks[index][i].evictCount == 0) {
                way_index = i;
                break;
            }
        }
        return;
    }

    // Update evicted counter
    void updateLRU(unsigned index, unsigned way_index) {
        unsigned x = blocks[index][way_index].evictCount;
        blocks[index][way_index].evictCount = num_of_ways - 1; 
        for (unsigned j = 0; j < num_of_ways; j++) {
            if (j != way_index && blocks[index][j].evictCount > x) {
                blocks[index][j].evictCount--;
            }
        }
        return;
    }

    // Insert block into cache
    void insertBlock(unsigned address, unsigned way, bool isDirty) {
        unsigned index = (address >> offset_size) & ((1 << set_size) - 1);
        unsigned tag = address >> (offset_size + set_size);
        blocks[index][way].valid = true;
        blocks[index][way].tag = tag;
        blocks[index][way].dirty = isDirty;
    }

    // Invalidate block
    bool invalidate(unsigned address) {
        unsigned index = (address >> offset_size) & ((1 << set_size) - 1);
        unsigned tag = address >> (offset_size + set_size);
        
        for (unsigned i = 0; i < num_of_ways; i++) {
            if (blocks[index][i].valid && blocks[index][i].tag == tag) {
                bool wasDirty = blocks[index][i].dirty;
                blocks[index][i].valid = false;
                blocks[index][i].dirty = false;
                return wasDirty;
            }
        }
        return false;
    }

    // Get block address?
    unsigned getBlockAddress(unsigned index, unsigned way_index) {
        return (blocks[index][way_index].tag << (set_size + offset_size)) | (index << offset_size);
    }

    // Check dirty
    bool isDirty(unsigned index, unsigned way_index) {
        return blocks[index][way_index].dirty;
    }

    // Set dirty 
    void setDirty(unsigned index, unsigned way_index) {
        blocks[index][way_index].dirty = true;
    }
    // get access time 
    unsigned getAccessTime() const { 
        return accessTime; 
    }
    // get index of address
    unsigned getIndex(unsigned address) {
        return (address >> offset_size) & ((1 << set_size) - 1);
    }

    // Check if block is valid
    bool isValid(unsigned index, unsigned way_index) {
        return blocks[index][way_index].valid;
    }
};
// calss cachesystem
class CacheSystem {
private:
    unsigned memCycle;
    unsigned wrAlloc;
    Cache L1, L2;
    double totalTime;
    double missesL1;
    double missesL2;
    double accessesL1;
    double accessesL2;

public:
    CacheSystem(unsigned memCycle, unsigned blockSize, unsigned L1Size, unsigned L2Size, unsigned L1Assoc, unsigned L2Assoc,
                unsigned L1Cycles, unsigned L2Cycles, unsigned wrAlloc)
        : memCycle(memCycle),
          wrAlloc(wrAlloc),
          L1(L1Size, blockSize, L1Assoc, L1Cycles),
          L2(L2Size, blockSize, L2Assoc, L2Cycles),
          totalTime(0),
          missesL1(0), 
          missesL2(0), 
          accessesL1(0), 
          accessesL2(0) {}
    // access to memory hir
    void access(unsigned address, char operation) {
        accessesL1++;//update l1 
        totalTime += L1.getAccessTime();//update total time
        unsigned updateLru2 =false; 
        bool snoop =false;
        unsigned evictedAddr;
        unsigned evictedAddr2; 
        bool  bylru=false;
        unsigned way1_index = 0;
        //check for hit 
        bool hit1 = L1.checkHit(address, way1_index);
        if (hit1) {
            if (operation == 'w') {
                L1.setDirty(L1.getIndex(address), way1_index);
            }
            L1.updateLRU(L1.getIndex(address), way1_index);
            return;
        }
        /////// miss l1/////
        missesL1++;// mis l1 
        accessesL2++;
        totalTime += L2.getAccessTime();

        unsigned way2_index = 0;
        //check hit l2
        bool hit2 = L2.checkHit(address, way2_index);
        //mis
        if (!hit2) {
            // L2 Miss
            missesL2++;
            totalTime += memCycle;
            
            if (wrAlloc == 1 || operation == 'r') {
                // Allocate in L2
                unsigned indexL2 = L2.getIndex(address);
                unsigned way_index=0;
                bool l2AllocWay = L2.findInvalidWay(indexL2, way_index); // find invalid
                
                if (!l2AllocWay) {
                    // L2 full
                    L2.findByEvictedCount(indexL2, way_index); // find evicted- lru
                }
                if (L2.isValid(indexL2, way_index)) {
                        //  invalidate from L1
                    unsigned evictedAddr = L2.getBlockAddress(indexL2, way_index);
                    bool wasDirty = L1.invalidate(evictedAddr);
                        
                    if (wasDirty) {
                        // Update L2 LRU for dirty eviction
                        //updateL2LRU(evictedAddr);
                        snoop=true; // fir snoop
                    }
                }
                
                L2.insertBlock(address, way_index, false); // update l2 block
                way2_index = way_index;
                updateLru2=true;
            }
        }

        if (wrAlloc == 1 || operation == 'r') {
            // Allocate in L1
            unsigned indexL1 = L1.getIndex(address);
            unsigned way_index1=0;
            bool l1AllocWay = L1.findInvalidWay(indexL1, way_index1);
            
            if (!l1AllocWay) {
                // L1 full, evict LRU
                L1.findByEvictedCount(indexL1, way_index1);
                
                if (L1.isValid(indexL1, way_index1) && L1.isDirty(indexL1, way_index1)) {
                    // Dirty eviction from L1 - update L2
                    evictedAddr2 = L1.getBlockAddress(indexL1, way_index1);
                    //updateL2LRU(evictedAddr2);
                    bylru=true;
                }
            }
            
            bool isDirty = (operation == 'w');
            L1.insertBlock(address, way_index1, isDirty); //update l1 block
            L1.updateLRU(indexL1, way_index1); // update lru of 1
        }
        
        // Update L2 LRU if we accessed it
        if (updateLru2 || hit2) {
            L2.updateLRU(L2.getIndex(address), way2_index);
        }
        if(snoop){
            updateL2LRU(evictedAddr); // snoop
        }
        if(bylru){
             updateL2LRU(evictedAddr2);// dirty update 
        }
        
    }
// update lru 2 by snoop or dirty 
    void updateL2LRU(unsigned address) {
        // Snoop operation - zero time cost
        unsigned way;
        bool l2Way_hit = L2.checkHit(address, way);
        if (l2Way_hit) {
            L2.updateLRU(L2.getIndex(address), way);
        }
    }
    //print statistics
    void print_statistics() const {
        printf("L1miss=%.03f ", (float)missesL1 / accessesL1);
        printf("L2miss=%.03f ", (float)missesL2 / accessesL2);
        printf("AccTimeAvg=%.03f\n", (float)totalTime / accessesL1);
    }
};
/*-------------------------------------------------------------------------------------------------------*/
int main(int argc, char **argv) {

	if (argc < 19) {
		cerr << "Not enough arguments" << endl;
		return 0;
	}
	// Get input arguments
	// File
	// Assuming it is the first argument
	char* fileString = argv[1];
	ifstream file(fileString); //input file stream
	string line;
	if (!file || !file.good()) {
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}
	unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;
	for (int i = 2; i < 19; i += 2) {
		string s(argv[i]);
		if (s == "--mem-cyc") {
			MemCyc = atoi(argv[i + 1]);
		} else if (s == "--bsize") {
			BSize = atoi(argv[i + 1]);
		} else if (s == "--l1-size") {
			L1Size = atoi(argv[i + 1]);
		} else if (s == "--l2-size") {
			L2Size = atoi(argv[i + 1]);
		} else if (s == "--l1-cyc") {
			L1Cyc = atoi(argv[i + 1]);
		} else if (s == "--l2-cyc") {
			L2Cyc = atoi(argv[i + 1]);
		} else if (s == "--l1-assoc") {
			L1Assoc = atoi(argv[i + 1]);
		} else if (s == "--l2-assoc") {
			L2Assoc = atoi(argv[i + 1]);
		} else if (s == "--wr-alloc") {
			WrAlloc = atoi(argv[i + 1]);
		} else {
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}
	CacheSystem cacheSystem(MemCyc, BSize, L1Size, L2Size, L1Assoc, L2Assoc,
                L1Cyc, L2Cyc, WrAlloc);
	while (getline(file, line)) {
		stringstream ss(line);
		string address;
		char operation = 0; // read (R) or write (W)
		if (!(ss >> operation >> address)) {
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}
		// DEBUG - remove this line
		//cout << "operation: " << operation;
		string cutAddress = address.substr(2); // Removing the "0x" part of the address
		// DEBUG - remove this line
		//cout << ", address (hex)" << cutAddress;
		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);
		// DEBUG - remove this line
		//cout << " (dec) " << num << endl;
		cacheSystem.access(num, operation);
	}
	//double L1MissRate;
	//double L2MissRate;
	//double avgAccTime;
	//printf("L1miss=%.03f ", L1MissRate);
	//printf("L2miss=%.03f ", L2MissRate);
	//printf("AccTimeAvg=%.03f\n", avgAccTime);
	cacheSystem.print_statistics();
	return 0;
}
/*--------------------------------------------------------------------------------------------------------------------------------*/

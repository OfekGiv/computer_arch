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
using std::vector;

struct cacheBlock {
    unsigned int tag;
    bool valid = 0;
    bool dirty = 0;
    unsigned int evictCount = 0;
};

class Cache {
public:
    Cache(unsigned cacheSize, unsigned blockSize, unsigned assoc, unsigned accessTime)
        :blockSize(blockSize), assoc(assoc), accessTime(accessTime) {
            sets = cacheSize / (blockSize * assoc);    
            blocks.resize(sets,vector<cacheBlock>(assoc));
            blockOffsetBits = log2(blockSize);
            setIndexBits = log2(sets); 
        }

    // Check cache hit
    // return true for hit, false for miss
    bool checkHit(unsigned address) {

        unsigned setIndex = (address >> blockOffsetBits) & ((1 << setIndexBits)-1);
        unsigned tag = address >> (blockOffsetBits + setIndexBits);

        for (auto& block : blocks[setIndex]){
            if (block.valid && block.tag == tag) {
                return true; // hit
            }
        }
        return false; // miss
    }
    
    // Update LRU if tags are matching
    // return 0 on success, return -1 when no matching tag was found
    int updateLRU(unsigned address) {

        unsigned setIndex = (address >> blockOffsetBits) & ((1 << setIndexBits)-1);
        unsigned tag = address >> (blockOffsetBits + setIndexBits);

        for (auto& block : blocks[setIndex]){
            if (block.valid && block.tag == tag) {
                unsigned int x = block.evictCount;
                block.evictCount = assoc - 1;
                for (auto& way : blocks[setIndex]){
                    if(block.tag != way.tag && (way.evictCount > x))
                        way.evictCount--;
                }
                return 0;
            }
        }
        return -1;
    }


    // Update dirty bit if tags are matching
    // return 0 on success, return -1 when no matching tag was found
    int updateDirtyBit(unsigned address) {

        unsigned setIndex = (address >> blockOffsetBits) & ((1 << setIndexBits)-1);
        unsigned tag = address >> (blockOffsetBits + setIndexBits);
        
        for (auto& block : blocks[setIndex]){
            if (block.valid && block.tag == tag) {
                block.dirty = 1;
                return 0; 
            }
        }
        return -1; 
    }

    // Evict blocks from cache and replace with new block
    // return 0 for clean eviction, return 1 for dirty eviction with dirty evicted address passed by reference
    int evictBlock(unsigned address, unsigned& evictedAddress) {

        unsigned isDirty = 0;
        unsigned setIndex = (address >> blockOffsetBits) & ((1 << setIndexBits)-1);
        unsigned tag = address >> (blockOffsetBits + setIndexBits);

        for (auto& block : blocks[setIndex]){
            // evict least recently used way with evictCount = 0
            if (block.evictCount == 0){

                // Check for dirty eviction
                if (block.dirty == 1){
                    isDirty = 1;
                }
                evictedAddress = (block.tag << (blockOffsetBits + setIndexBits)) | (setIndex << blockOffsetBits);

                block.tag = tag;
                // LRU update algorithm
                unsigned int x = block.evictCount;
                block.evictCount = assoc - 1;
                for (auto& way : blocks[setIndex]){
                    if(block.tag != way.tag && (way.evictCount > x)){
                            way.evictCount--;
                    }
                return isDirty; // clean eviction 
                }
            }
        }
    }

    // Check if set is full
    // return true if full, false if not full
    bool checkSetFull(unsigned address) {
        
        unsigned setIndex = (address >> blockOffsetBits) & ((1 << setIndexBits)-1);

        for (auto& block : blocks[setIndex]){
            if (!block.valid) {
                return false; // Set in not full
            }
        }
        return true; // Set is full
    }
    
    // Insert a new block to an empty cache line, update valid bit and update LRU
    // return 0 on success, return -1 if no empty line was found
    int updateValidBit(unsigned address) {

        unsigned setIndex = (address >> blockOffsetBits) & ((1 << setIndexBits)-1);
        unsigned tag = address >> (blockOffsetBits + setIndexBits);
        
        for (auto& block : blocks[setIndex]){
            if (!block.valid) {
                block.tag = tag;
                block.valid = 1;
                unsigned int x = block.evictCount;
                block.evictCount = assoc - 1;
                for (auto& way : blocks[setIndex]){
                    if(block.tag != way.tag && (way.evictCount > x))
                        way.evictCount--;
                }
                return 0; 
            }
        }
        return -1; 
    }

    unsigned getAccessTime() const { return accessTime;}

private:
    unsigned blockSize, assoc, sets;
    unsigned blockOffsetBits;
    unsigned setIndexBits; 
    unsigned accessTime;
    vector<vector<cacheBlock> > blocks;
};


class cacheSystem {

public:
    cacheSystem(unsigned memCycle, unsigned blockSize, unsigned L1Size, unsigned L2Size, unsigned L1Assoc, unsigned L2Assoc,
                unsigned L1Cycles, unsigned L2Cycles, unsigned wrAlloc)
        : memCycle(memCycle), wrAlloc(wrAlloc),
          L1(L1Size, blockSize, L1Assoc, L1Cycles),
          L2(L2Size, blockSize, L2Assoc, L2Cycles),
          blockSize(blockSize) {}
    
    void access(unsigned address, char operation){

        unsigned evictedAddress;

        // Access L1
        AccessL1++;
        totalCycles += L1.getAccessTime();

        // L1 hit
        if (L1.checkHit(address)) {
            // Update L1 LRU
            if (!L1.updateLRU(address)) {
                cout << __LINE__ << ": Could not update L1 LRU" << endl;
                return;
            }

            if (operation == 'w') {
                //Update L1 dirty bit
                if (!L1.updateDirtyBit(address)) {
                    cout << __LINE__ << ": Could not update dirty bit" << endl;
                    return;
                }
            }

            return;
        }
        // L1 miss
        missesL1++;
        // L2 access
        AccessL2++;
        totalCycles += L2.getAccessTime();

        if (L2.checkHit(address)) {
            // Update L2 LRU
            if (!L2.updateLRU(address)) {
                cout << __LINE__ << ": Could not update L2 LRU" << endl;
                return;
            }
            
            if ((operation == 'w' && wrAlloc == 1)) {
                if (wrAlloc) {
                    // Update L2 dirty bit
                    if (!L2.updateDirtyBit(address)) {
                        cout << __LINE__ << ": Could not update dirty bit" << endl;
                        return;
                    }
                }
                // Update L1 dirty bit
                if (L1.evictBlock(address,evictedAddress)) {
                    if (!L1.updateDirtyBit(address)) {
                        cout << __LINE__ << ": Could not update dirty bit" << endl;
                        return;
                    }
                }
            }
            else if (operation == 'r') {
                // Update L1 dirty bit
                if (L1.evictBlock(address,evictedAddress)) {
                    if (!L1.updateDirtyBit(address)) {
                        cout << __LINE__ << ": Could not update dirty bit" << endl;
                        return;
                    }
                }

            }
            return;
        }

        // L2 miss
        missesL2++;
        // Memory access
        totalCycles += memCycle;

        if (operation == 'w') {
            if (wrAlloc == 1) {
                // L1 miss and L2 miss, write access with write allocate
                // Evict L2 line 
                L2.evictBlock(address, evictedAddress);
                // Evict L1 line
                if (L1.evictBlock(address, evictedAddress)) {
                    // Update dirty evicted line in L2
                    L2.updateLRU(evictedAddress);
                }

            }
        }
        else {
            if (L2.checkSetFull(address)) {
                L2.evictBlock(address,evictedAddress);
            }
            else {
                if (!L2.updateValidBit(address)){
                    cout << __LINE__ << ": No empty cache line was found in L2" << endl;
                    return;
                }
            }


            if (L1.checkSetFull(address)) {
                if (L1.evictBlock(address, evictedAddress)) {
                    // Update dirty evicted line in L2
                    L2.updateLRU(evictedAddress);
                }
            }
            else {
                if (!L1.updateValidBit(address)){
                    cout << __LINE__ << ": No empty cache line was found in L1" << endl;
                    return;
                }
            }
        }
    }


    double getTotalCycles() const { return totalCycles;}
    double getL1MissRate() const { return missesL1/AccessL1;}
    double getL2MissRate() const { return missesL2/AccessL2;}

private:
    unsigned memCycle, wrAlloc;
    unsigned blockSize;
    Cache L1,L2;
    unsigned totalCycles = 0;
    unsigned AccessL1 = 0;
    unsigned AccessL2 = 0;
    unsigned missesL1 = 0;
    unsigned missesL2 = 0;

};

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
		cout << "operation: " << operation;

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		// DEBUG - remove this line
		cout << ", address (hex)" << cutAddress;

		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);

		// DEBUG - remove this line
		cout << " (dec) " << num << endl;

	}

    cacheSystem cache(MemCyc,BSize,L1Size,L2Size,L1Assoc,L2Assoc,L1Cyc,L2Cyc,WrAlloc); 

	double L1MissRate = cache.getL1MissRate();
	double L2MissRate = cache.getL2MissRate();
	double avgAccTime = cache.getTotalCycles();

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}

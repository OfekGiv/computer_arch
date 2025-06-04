#include <iomanip>
#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

#define ADDRESS_SIZE 32 //address is 32 bit long

struct cacheBlock {
    unsigned tag = 0;
    bool valid = 0;
    bool dirty = 0;
    unsigned evictCount = 0;
};

class Cache {
private:
    unsigned cacheSize;
    unsigned blockSize, assoc;
    unsigned accessTime;
    unsigned num_of_sets; //number of sets
    unsigned num_of_ways; //number of ways
    unsigned set_size; // set size in bits
    unsigned offset_size; //offset size in bits
    unsigned tag_size; // tag size in bits
    vector<vector<cacheBlock>> blocks;

public:
    Cache(unsigned cacheSize, unsigned blockSize, unsigned assoc, unsigned accessTime)
        : cacheSize(cacheSize), blockSize(blockSize), assoc(assoc), accessTime(accessTime) {
        set_size = cacheSize - blockSize - assoc;
        num_of_sets = 1 << set_size;
        num_of_ways = 1 << assoc; 
        offset_size = blockSize; // in bits- log2(bytes)
        tag_size = ADDRESS_SIZE - offset_size - set_size; // calc    
        blocks.resize(num_of_sets, vector<cacheBlock>(num_of_ways));
    }

    // Check for hit
    bool checkHit(unsigned address, unsigned& way_index) {
        unsigned index = (address >> offset_size) & ((1 << set_size) - 1);
        unsigned tag = address >> (offset_size + set_size);
            
        for (unsigned i = 0; i < num_of_ways; i++) {
            if (blocks[index][i].valid && blocks[index][i].tag == tag) {
                way_index = i;
                return true;
            }
        }
        return false;
    }
    
    // Find invalid way
    bool findInvalidWay(unsigned index, unsigned& way_index) {
        for (unsigned i = 0; i < num_of_ways; i++) {
            if (!blocks[index][i].valid) {
                way_index = i;
                return true;
            }
        }
        return false;
    }

    // find oldest 
    void findByEvictedCount(unsigned index, unsigned& way_index) {
        for (unsigned i = 0; i < num_of_ways; i++) {
            if (blocks[index][i].evictCount == 0) {
                way_index = i;
                break;
            }
        }
        return;
    }

    // Update evicted counte
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

    unsigned getAccessTime() const { 
        return accessTime; 
    }
    
    unsigned getIndex(unsigned address) {
        return (address >> offset_size) & ((1 << set_size) - 1);
    }

    // Check if block is valid
    bool isValid(unsigned index, unsigned way_index) {
        return blocks[index][way_index].valid;
    }
};

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
    
    void access(unsigned address, char operation) {
        accessesL1++;
        totalTime += L1.getAccessTime();
        unsigned updateLru2 =false;

        unsigned way1_index = 0;
        bool hit1 = L1.checkHit(address, way1_index);
        if (hit1) {
            if (operation == 'w') {
                L1.setDirty(L1.getIndex(address), way1_index);
            }
            L1.updateLRU(L1.getIndex(address), way1_index);
            return;
        }

        /////// miss l1/////
        missesL1++;
        accessesL2++;
        totalTime += L2.getAccessTime();

        unsigned way2_index = 0;
        bool hit2 = L2.checkHit(address, way2_index);

        if (!hit2) {
            // L2 Miss
            missesL2++;
            totalTime += memCycle;
            
            if (wrAlloc == 1 || operation == 'r') {
                // Allocate in L2
                unsigned indexL2 = L2.getIndex(address);
                unsigned way_index=0;
                bool l2AllocWay = L2.findInvalidWay(indexL2, way_index);
                
                if (!l2AllocWay) {
                    // L2 full
                    L2.findByEvictedCount(indexL2, way_index);
                }
                if (L2.isValid(indexL2, way_index)) {
                        //  invalidate from L1
                    unsigned evictedAddr = L2.getBlockAddress(indexL2, way_index);
                    bool wasDirty = L1.invalidate(evictedAddr);
                        
                    if (wasDirty) {
                        // Update L2 LRU for dirty eviction
                        updateL2LRU(evictedAddr);
                    }
                }
                
                L2.insertBlock(address, way_index, false);
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
                    unsigned evictedAddr = L1.getBlockAddress(indexL1, way_index1);
                    updateL2LRU(evictedAddr);
                }
            }
            
            bool isDirty = (operation == 'w');
            L1.insertBlock(address, way_index1, isDirty);
            L1.updateLRU(indexL1, way_index1);
        }
        
        // Update L2 LRU if we accessed it
        if (updateLru2 || hit2) {
            L2.updateLRU(L2.getIndex(address), way2_index);
        }
    }

    void updateL2LRU(unsigned address) {
        // Snoop operation - zero time cost
        unsigned way;
        bool l2Way_hit = L2.checkHit(address, way);
        if (l2Way_hit) {
            L2.updateLRU(L2.getIndex(address), way);
        }
    }

    void print_statistics() const {
        printf("L1miss=%.3f ", (float)missesL1 / accessesL1);
        printf("L2miss=%.3f ", (float)missesL2 / accessesL2);
        printf("AccTimeAvg=%.3f\n", (float)totalTime / accessesL1);
    }
};


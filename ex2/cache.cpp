#include <iostream>
#include <vector>

using namespace std;

struct cacheBlock {
    unsigned int tag;
    bool valid = 0;
    bool dirty = 0;
    unsigned int evictCount = 0;
}

class Cache {
public:
    Cache(unsigned cacheSize, unsigned blockSize, unsigned assoc, unsigned accessTime)
        :blockSize(blockSize), assoc(assoc), accessTime(accessTime) {
            sets = cacheSize / (blockSize * assoc);    
            blocks.resize(sets,vector<cacheBlock>(assoc));
        }

    void access(unsigned address,char operation, bool& hit, bool& dirtyEviction, unsigned& evictedAddress) {
        
        unsigned blockOffsetBits = log2(blockSize);
        unsigned setIndexBits = log2(sets); 
        unsigned index = (address >> blockOffsetBits) & ((1 << setIndexBits)-1);
        unsigned tag = address >> (blockOffsetBits + setIndexBits);
        bool isEvicted;

        // check for hit
        for (auto& block : blocks[index]){
            // hit
            if (block.valid && block.tag == tag) {
                hit = true;
                // In case of write operation set diry bit
                if (operation == 'w')
                    block.dirty = 1;
                // LRU update algorithm
                unsigned int x = block.evictCount;
                block.evictCount = assoc - 1;
                for (auto& way : blocks[index]){
                    if(block != way && (way.evictCount > x)
                        way.evictCount--;
                }
                return; // no eviction
            }
        }

        // miss
        hit = false; 
        // In both L1 and L2 caches evict block in case all blocks are occupied
        // Check if cache is fully occupied
        for (auto& block : blocks[index]){

            // If not all occupied, insert block to the next free way
            if (!block.valid){
                block.valid = true;
                return; // no eviction
            }
        }

        // If all occupied, evict block according to LRU counters
        for (auto& block : blocks[index]){
            // evict least recently used way with evictCount = 0
            if (block.evictCount == 0){
                // Check for dirty eviction
                if (block.dirty == 1){
                    dirtyEviction = true;
                }
                evictedAddress = (block.tag << (blockOffsetBits + setIndexBits)) | (index << blockOffsetBits);
                // LRU update algorithm
                unsigned int x = block.evictCount;
                block.evictCount = assoc - 1;
                for (auto& way : blocks[index]){
                    if(block != way && (way.evictCount > x)){
                            way.evictCount--;
                    }
                return; // eviction
                }
            }
        }

        return;
    }

    unsigned getAccessTime() const { return accessTime;}

private:
    unsigned blockSize, assoc, sets;
    unsigned accessTime;
    vector<vector<cacheBlock>> blocks;
};


class cacheSystem {

public:
    cacheSystem(unsigned memCycle, unsigned blockSize, unsigned L1Size, unsigned L2Size, unsigned L1Assoc, unsigned L2Assoc,
                unsigned L1Cycles, unsigned L2Cycles, unsigned wrAlloc)
        : memCycle(memCycle), wrAlloc(wrAlloc),
          L1(L1Size, blockSize, L1Assoc, L1Cycles),
          L2(L2Size, blockSize, L2Alloc, L2Cycles),
          blockSize(blockSize) {}
    
    void access(unsigned address, char operation){
        bool hitL1;
        bool hitL2;
        bool dirtyEvicted = false;
        unsigned evictedAddr;

        // L1 access
        L1.access(address,operation,hitL1, dirtyEvicted, evictedAddr);

        totalCycles += L1.getAccessTime();

        if (hitL1){
            return;
        }
        
        bool dummy1,dummy2;
        unsigned dummy3;
        if (dirtyEvicted){
            L2.access(evictedAddr,'w', dummy1, dummy2, dummy3 );
            totalCycles += L2.getAccessTime();
        }

        missesL1++;

        // L2 access
        L2.access(address,operation,hitL2, dirtyEvicted, evictedAddr);

        totalCycles += L2.getAccessTime();

        if (hitL2){
            return;
        }

        // L2 miss
        missesL2++;
        totalCycles += memCycle;

        if (!hitL2 && operation == 'w' && wrAlloc == 0) {
            // no write allocate
            return;
        }
        
        // Update L1 on write-allocate or read miss
        L1.access(address, operation, dummy1, dummy2, dummy3);
        totalCycles += L1.getAccessTime();


    }

private:
    unsigned memCycle, wrAlloc;
    unsigned blockSize;
    Cache L1,L2;
    unsigned totalCycles = 0;
    unsigned missesL1 = 0;
    unsigned missesL2 = 0;


};




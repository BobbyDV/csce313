#include "BuddyAllocator.h"
#include <iostream>

//LinkedList Functions
LinkedList::LinkedList()
{
  listLength = 0;
}
int LinkedList::getListLength()
{
  return listLength;
}
BlockHeader *LinkedList::getHead()
{
  return head;
}
void LinkedList::insert(BlockHeader *b)
{
  if(!head)
  {
    b->next = nullptr;
  }
  else
  {
    b->next = head;
  }
  head = b;
  b->free = true;
  listLength = listLength + 1;
}
void LinkedList::remove(BlockHeader *b)
{
  BlockHeader *temp = head;

  //if the head is the node to be deleted
  if (temp == b)
  {
    if (temp->next != nullptr)
    {
      head = temp->next;
      listLength -= 1;
      b->free = false;
    }
    else
    {
      head = nullptr;
      listLength = 0;
    }
  }

  //if head wasnt the node to be deleted
  else
  {
    //while b still exists
    for(uint i = 1; i < listLength; i++)
    {
      if(temp->next == b)
      {
        //next node is b
        if(temp->next->next != nullptr)
        {
          temp->next = temp->next->next;
          listLength -= 1;
          b->free = false;
        }
        else
        {
          temp->next = nullptr;
          listLength -= 1;
          b->free = false;
        }
      }
      //if not, keep looking for b
      else
      {
        temp = temp->next;
      }
      
    }
  }
}

//BuddyAllocator functions
BuddyAllocator::BuddyAllocator(int _basic_block_size, int _total_memory_length)
{
  int i = 1;
  while ((_basic_block_size * i) < _total_memory_length)
  {
    i = i * 2;
  }
  totalMemory = _basic_block_size * i;

  //initial block will be maximum memory
  start = new BlockHeader[int(totalMemory)];

  //first node points to the head
  BlockHeader *blheader = (BlockHeader *)start;
  blheader->free = true;
  blheader->block_size = totalMemory;
  blheader->next = nullptr;
  smallestBlock = _basic_block_size;
  //max number of basic block sizes available
  freeListSize = int(log2(totalMemory / smallestBlock)) + 1;
  //linked list with length equal to max number of smallest blocks
  freeList = new LinkedList[freeListSize];
  //initialize first node
  freeList[this->nextFreeBlock(totalMemory)].insert(blheader);
}

//destructor
BuddyAllocator::~BuddyAllocator()
{
  //deallocation
  delete[] start;
  delete[] freeList;
}

//return the next free block for the size needed
int BuddyAllocator::nextFreeBlock(uint sizeNeeded)
{
  return freeListSize - ((int(log2(totalMemory / sizeNeeded)) + 1));
}

//returns the length of the free list
int BuddyAllocator::getFreeListLength()
{
  return freeListSize;
}

//returns the free list
LinkedList *BuddyAllocator::getFreeList()
{
  return freeList;
}

//returns the address of the allocated block
void *BuddyAllocator::alloc(int length)
{
  int i = 0;
  int j = 1;

  //sizeof() returns size of the object in bytes
  if (length > totalMemory - sizeof(BlockHeader))
  {
    return nullptr;
  }

  //figure out the nearest power of 2 block size for initiation
  while ((smallestBlock * j) - sizeof(BlockHeader) < length)
  {
    j = j * 2;
    i = i + 1;
  }
  int largestBlock = smallestBlock * j;

  //getListLength returns 0
  if (freeList[i].getListLength() > 0)
  {
    BlockHeader *block = freeList[i].getHead();
    block->free = false;
    block->next = nullptr;
    freeList[i].remove(block);
    char *blockReturn = (char *)block;
    return (char *)(blockReturn + sizeof(BlockHeader));
  }

  else
  {
    BlockHeader *splitBlock;
    //char* splitBlock;
    while (freeList[i].getListLength() == 0)
    {
      i++;
      if (i >= freeListSize)
      {
        return nullptr;
      }
    }
    int currentBlockSize = freeList[i].getHead()->block_size;

    while (largestBlock != currentBlockSize)
    {
      //char* block = (char*) freeList[nextFreeBlock(currentBlockSize)].getHead();
      BlockHeader *block = (BlockHeader *)freeList[nextFreeBlock(currentBlockSize)].getHead();
      splitBlock = this->split(block);
      block = splitBlock;
      // block = (BlockHeader*) splitBlock;
      currentBlockSize = currentBlockSize / 2;
    }

    freeList[nextFreeBlock(currentBlockSize)].remove((BlockHeader *)splitBlock);
    BlockHeader *splitBlockHeader = (BlockHeader *)splitBlock;
    char *splitBlock2 = (char *)splitBlock;
    return (splitBlock2 + sizeof(BlockHeader));
  }
}

void BuddyAllocator::free(void* a)
{
  //get the start of the memory taken up by the blockheader
  BlockHeader *startingAddress = (BlockHeader*)((char*)a - sizeof(BlockHeader));
  BlockHeader *bh = startingAddress;
  freeList[nextFreeBlock(bh->block_size)].insert(bh);
  bool merging = true;
  uint newSize = bh->block_size;
  //merge all blocks back togetheras long as there is> 1 split blocks
  while (merging == true)
  {

    BlockHeader *buddyBlockHeader = getbuddy(startingAddress);
    if (arebuddies(startingAddress, buddyBlockHeader))
    {
      startingAddress = merge(startingAddress, buddyBlockHeader);
      bh = (BlockHeader *)startingAddress;
    }

    else
    {
      merging = false;
    }
  }
}

//returns the buddy of an address
BlockHeader *BuddyAllocator::getbuddy(BlockHeader *addr)
{
  uint blockSize = addr->block_size;
  void *buddy = ((addr - start) ^ blockSize) + start;
  return ((addr - start) ^ blockSize) + start;
}

//figures out if two blocks are buddies
bool BuddyAllocator::arebuddies(BlockHeader *block1, BlockHeader *block2)
{
  bool x = block2->free && block1->block_size == block2->block_size;
  return x;
}

//merges two blocks together
BlockHeader *BuddyAllocator::merge(BlockHeader *block1, BlockHeader *block2)
{
  BlockHeader *leftBuddy;
  BlockHeader *rightBuddy;
  if (block1 > block2)
  {
    leftBuddy = block2;
    rightBuddy = block1;
  }
  else if (block1 < block2)
  {
    leftBuddy = block1;
    rightBuddy = block2;
  }
  uint originalSize = leftBuddy->block_size;
  uint newSize = originalSize * 2;

  freeList[this->nextFreeBlock(originalSize)].remove(leftBuddy);
  freeList[this->nextFreeBlock(originalSize)].remove(rightBuddy);
  freeList[this->nextFreeBlock(newSize)].insert(leftBuddy);

  BlockHeader *mergeBlock = leftBuddy;
  mergeBlock->block_size = newSize;
  return mergeBlock;
}

BlockHeader *BuddyAllocator::split(BlockHeader *block)
{
  BlockHeader *leftBuddy = block;
  uint originalSize = leftBuddy->block_size;

  //remove original block
  freeList[this->nextFreeBlock(originalSize)].remove(leftBuddy);
  uint splitSize = originalSize / 2;
  BlockHeader *rightBuddy = block + splitSize;
  leftBuddy->block_size = splitSize;
  leftBuddy->next = nullptr;
  rightBuddy->block_size = splitSize;
  rightBuddy->next = nullptr;
  //insert left block and right block
  freeList[this->nextFreeBlock(splitSize)].insert(leftBuddy);
  freeList[this->nextFreeBlock(splitSize)].insert(rightBuddy);
  return leftBuddy;
}

void BuddyAllocator::printlist()
{
  cout << "Printing the Freelist in the format \"[index] (block size) : # of blocks\"" << endl;
  int64_t total_free_memory = 0;
  for (int i = 0; i < freeListSize; i++)
  {
    int blocksize = ((1 << i) * smallestBlock);    // all blocks at this level are this size
    cout << "[" << i << "] (" << blocksize << ") : "; // block size at index should always be 2^i * bbs
    int count = 0;
    BlockHeader *b = freeList[i].getHead();
    // go through the list from head to tail and count
    while (b)
    {
      total_free_memory += blocksize;
      count++;
      // block size at index should always be 2^i * bbs
      // checking to make sure that the block is not out of place
      if (b->block_size != blocksize)
      {
        cerr << "ERROR:: Block is in a wrong list" << endl;
        exit(-1);
      }
      b = b->next;
    }
    cout << count << endl;
    cout << "Amount of available free memory: " << total_free_memory << " byes" << endl;
  }
}

#include "Ackerman.h"
#include "BuddyAllocator.h"
#include <iostream>
#include <cstdlib>
#include <unistd.h>

using namespace std;

void easytest(BuddyAllocator *ba)
{
  // be creative here
  // know what to expect after every allocation/deallocation cycle

  // here are a few examples
  ba->printlist();
  // allocating a byte
  char *mem = (char *)ba->alloc(1);
  // now print again, how should the list look now
  ba->printlist();

  ba->free(mem);   // give back the memory you just allocated
  ba->printlist(); // shouldn't the list now look like as in the beginning
}

int main(int argc, char **argv)
{

  int basic_block_size = 128, memory_length = 512 * 1024;
  //getopt for command line arguments

  bool compatible = true;

  //getopt for command line
  int x;

  try
  {
    while((x = getopt(argc, argv, "b:t:")) != -1)
    {
      switch(x)
      {
        //set size of basic_block_size
        case 'b':
          basic_block_size = atoi(optarg);
          cout << "Set Basic Block Size: " << basic_block_size << endl;
          break;
        
        //set memory_length
        case 't':
          memory_length = atoi(optarg);
          cout << "Memory Length set to: " << memory_length << endl;
          break;
        
        case '?':
          cout << "Error in input" << endl;
          compatible = false;
      }

      if(basic_block_size > memory_length)
      {
        cout << "Basic block size is greater than memory length" << endl;
        compatible = false;
      }

      if(compatible)
      {
        // create memory manager
        BuddyAllocator *allocator = new BuddyAllocator(basic_block_size, memory_length);

        // the following won't print anything until you start using FreeList and replace the "new" with your own implementation
        easytest(allocator);

        // stress-test the memory manager, do this only after you are done with small test cases
        Ackerman *am = new Ackerman();
        am->test(allocator); // this is the full-fledged test.

        // destroy memory manager
        delete allocator;
      }
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
  }
}

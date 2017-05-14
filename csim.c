
#include "cachelab.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

int miss = 0, hit = 0, evict = 0;

struct Set {
	long unsigned int tag;
	int blockOffset;
	struct timeval time;
	int valid;
};

void load(char access_type, long unsigned int tag, int numOfLines, int setIndex, int bOffset, struct Set ** cache, char* buf)
{
    //printf("Tag is: %d, blockOffset is: %d, set index: %d\n", tag, bOffset, setIndex);
	// find tag match
	// find emtpy
	// get LUR entry
    int found = 0;
    for (int i = 0; i < numOfLines; ++i)
    {
        struct Set* s = &cache[setIndex][i];
        if (s->valid == 1 && s->tag == tag)
        {
            hit++;
			printf("I am hit!!! my index is: %d\n", i);
			strcat(buf, "hit ");
            found = 1;
            s->valid = 1;
			s->blockOffset = bOffset;
			// update time
            gettimeofday(&s->time, NULL);
            break;
        }

    }
    if (found == 0)
    {
        miss++;
		strcat(buf, "miss ");
        for (int i = 0; i < numOfLines; ++i)
        {
        	struct Set* s = &cache[setIndex][i];
        	if (s->valid == 0)
        	{
            	s->tag = tag;
            	s->blockOffset = bOffset;
            	s->valid = 1;
            	found = 1;
            	// update time
            	gettimeofday(&s->time, NULL);
            	break;
        	}
        }

        if (found == 0)
        {
            int lruIndex = 0;
            struct timeval curMin;
            gettimeofday(&curMin, NULL);
            for (int i = 0; i < numOfLines; ++i)
            {
                struct Set* s = &cache[setIndex][i];
                if (s->valid == 1)
                {
					if (s->time.tv_sec < curMin.tv_sec)
					{
						curMin = s->time;
						lruIndex = i;
					}
					else if (s->time.tv_sec == curMin.tv_sec && s->time.tv_usec < curMin.tv_usec)
                    {
                        curMin = s->time;
                        lruIndex = i;
                    }
                }
            }
			printf("i am kicking index: %d\n", lruIndex);
            struct Set* s = &cache[setIndex][lruIndex];
            s->tag = tag;
            s->blockOffset = bOffset;
            s->valid = 1;
            found = 1;
            gettimeofday(&s->time, NULL);
            strcat(buf, "eviction ");
			evict++;
        }
    }
}

int main(int argc, char *argv[])
{
	// parse args
	int numOfSet = 0;
	int setBits = 0;
	int numOfLines = 0;
	int numOfBlocks = 0;
	char * traceFile = NULL;

	int opt = 0;
	while (-1 != (opt = getopt(argc, argv, "s:E:b:t:")))
	{
		switch (opt) {
			case 's':
				// number of sets
				setBits = atoi(optarg);
				numOfSet = pow(2, atoi(optarg));
				printf("number of sets: %d\n", numOfSet);
				break;
			case 'E':
				// number of lines per set
				numOfLines = atoi(optarg);
                printf("number of lines per set: %d\n", numOfLines);
				break;
			case 'b':
				// number of block bits
				numOfBlocks = atoi(optarg);
                printf("number of block bits: %d\n", numOfBlocks);
				break;
			case 't':
				// trace file
				traceFile = optarg;
				printf("The trace files is: %s\n", traceFile);
				break;
			default:
				printf("unknown argument");
		}
	}

	// allocate array
	struct Set ** cache = malloc(sizeof(struct Set*) * numOfSet);
	if (cache == NULL)
	{
		printf("malloc failed!");
		return 0;
	}

	for (int i = 0; i < numOfSet; ++i){
		cache[i] = malloc(numOfLines * sizeof(struct Set));
		for (int j = 0; j < numOfLines; ++j)
		{
			struct Set* s = &cache[i][j];
			s->valid = 0;
			s->tag = 0;
		}

	}

	// parse trace file
        FILE *pFile;
	pFile = fopen(traceFile, "r");
	if (pFile == NULL) {
		printf("File not exist!");
		return 0;
	}

	// caculate bits
	int bBits = numOfBlocks;
	int tagEndIndex = bBits + setBits;

	char access_type;
	unsigned long addr = 0;
	int size = 0;
	while (fscanf(pFile, " %c %lx,%d", &access_type, &addr, &size) > 0)
	{
		//printf(" %c %lx,%d, tagShifted:%d\n", access_type, addr, size, tagEndIndex);
		// tag
		long unsigned int tag = addr >> tagEndIndex;
		// block offset
		unsigned mask = (1L << bBits) - 1;
		int bOffset = addr & mask; // bitMask

		// set index, bBits is the startIndex, we want to only get the set bits
	    unsigned setMask = ((1L << setBits) - 1) << bBits; // starting at bBits+1
		int setIndex = (addr & setMask) >> bBits; // change xxx000 to xxx, 000 is the block bits


		if (access_type == 'L')
		{
            char buf[12];
			strcpy(buf, "");
			load(access_type, tag, numOfLines, setIndex, bOffset, cache, buf);
			printf(" %c %lx,%d %s\n", access_type, addr, size, buf);
			printf("Tag is: %lx, blockOffset is: %d, set index: %d\n", tag, bOffset, setIndex);

			//printf("The occupied set index is: %d, the block offset is: %d\n", setIndex, bOffset);

		}
		else if (access_type == 'S')
		{
			char buf[12];
			strcpy(buf, "");

			load(access_type, tag, numOfLines, setIndex, bOffset, cache, buf);

			printf(" %c %lx,%d, %s\n", access_type, addr, size, buf);
			printf("Tag is: %lx, blockOffset is: %d, set index: %d\n", tag, bOffset, setIndex);

			//printf("The occupied set index is: %d, the block offset is: %d\n", setIndex, bOffset);
		}
		else if (access_type == 'M')
		{
			// modify, call load and then store
            char buf[12];
			strcpy(buf, "");

			load(access_type, tag, numOfLines, setIndex, bOffset, cache, buf);

			load(access_type, tag, numOfLines, setIndex, bOffset, cache, buf);
		    printf(" %c %lx,%d, %s\n", access_type, addr, size, buf);
		    printf("Tag is: %lx, blockOffset is: %d, set index: %d\n", tag, bOffset, setIndex);
			//printf("The occupied set index is: %d, the block offset is: %d\n", setIndex, bOffset);
		}

	}


	printSummary(hit, miss, evict);
	free(cache);
	return 0;
}

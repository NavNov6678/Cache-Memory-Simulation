#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h> // did not end up using
#include <errno.h>
// other headers as needed

#define ADDRESS_LENGTH 64  // 64-bit memory addressing

// other variables as needed
static int GLOBAL_VERBOSE = 0;
static int Global_hits = 0;
static int Global_misses = 0;
static int Global_evicts = 0;
static unsigned long Global_Ticks=0;

/*
 * this function provides a standard way for your cache
 * simulator to display its final statistics (i.e., hit and miss)
 */
void print_summary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

/*
 * print usage info
 */
void print_usage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/trace01.dat\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/trace01.dat\n", argv[0]);
    exit(0);
}

/*
 * starting point
 */

// types of cache
//valid, tag, last used tick
// the tick is the timestamp for when a line of a cache was last used
typedef struct {
    int value;
    unsigned long tag;
    unsigned long t;
}Line;
// the number of lines
typedef struct {
    Line *L;
}Set;
//making 2^s sets
typedef struct {
    Set *S;
    int s;
    int E;
    int b;
    unsigned long nS;

}Cache;

// making globals

// allocation for the cache
static Cache make_cache(int s, int E, int b) {
    // initialize struct fields
    Cache c; // make a local cache
    c.S = NULL; // initialize the points for the sets to null
    c.s = s; // store the number of set indext bits in local
    c.E = E; // store the number of lines per set
    c.b = b; //store the block offset bits
    c.nS = (1UL << s); // figure out the number of sets.
    // I didnt know before this that you could just type 1UL as an unsigned long
    c.S = (Set*) calloc(c.nS, sizeof(Set)); // now c.S pints to the array of sets in cache
    if (!c.S) {
        perror("calloc sets"); exit(1);
    }
    for (unsigned long i = 0; i < c.nS; i++) {
        c.S[i].L = (Line*) calloc((size_t)E, sizeof(Line));
        if (!c.S[i].L) {
            perror("calloc lines"); exit(1);

        }

    }
    return c;
}
static void free_da_cache(Cache *c) {
    if (!c || !c->S) {
        return;
    }// check if the pointer is null
    for (unsigned long i = 0; i < c->nS; i++) {
        free(c->S[i].L);

    }
    free(c->S);
    c->S = NULL;
}

static int mod_cache(Cache *c, unsigned long address, int *eviction) {
    *eviction = 0;
    // handles the corner case that s=0 which means there are no set bits.
    unsigned long set = (c->s ? ((address >> c->b) & ((1UL << c->s)-1)): 0);
    // the tag is the highest bits in the address, so we need to make sure we remove the block offset bits
    unsigned long tag = address >> (c->s + c->b);
    Set *S = &c->S[set];

    // if there is one hit
    for (int i =0; i < c->E; i++) {
        if (S->L[i].value && S->L[i].tag == tag) {
            Global_hits++;
            S->L[i].t = ++Global_Ticks;
            return 1;


        }
    }

    // if there is a miss it prefers when it is empty, otherwise least recently used
        Global_misses++;
        int empty = -1;
        int leastRecent_j = 0;
        unsigned long leastRecent_tag = S->L[0].t;
        for (int j = 0; j < c->E; j++) {
            if (!S->L[j].value && empty ==-1) {
                empty = j;
            }
            if (S->L[j].t < leastRecent_tag) {
                leastRecent_tag = S->L[j].t;
                leastRecent_j = j;
            }
        }
        int index;
        if (empty != -1) {
            index = empty;
        }
        else {
            index = leastRecent_j;
            Global_evicts++;
            *eviction = 1;

        }
        S->L[index].value = 1;
        S->L[index].tag = tag;
        S->L[index].t = ++Global_Ticks;
        return 0;

}

static void do_trace(Cache *c, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Can't open file %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    char buffer[128], op;
    unsigned long address;
    int size;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (buffer[0] == 'I') {
            continue;
        }
        if (sscanf(buffer, " %c %lx,%d", &op, &address, &size) != 3) {
            continue;
        }
        if (op != 'L' && op != 'S' && op != 'M') {
            continue;
        }
        int EvicFlag = 0;
        if (GLOBAL_VERBOSE) {
            printf("%c %lx,%d", op, address, size);
        }
        if (op == 'M') {
            int h1 = mod_cache(c, address, &EvicFlag);
            if (GLOBAL_VERBOSE) { // first go through
                printf("%c %lx,%d", op, address, size);

                printf(h1 ? "hit ": (EvicFlag ? "miss eviction " : "miss")); // print if it was a hit, missed but forced an eviction, or just missed


            }
            EvicFlag = 0;
            (void)mod_cache(c, address, &EvicFlag);
            if (GLOBAL_VERBOSE) { // second go through
                puts("hit"); // always a hit
            }

        }
        else { //
            int h = mod_cache(c, address, &EvicFlag);
            if (GLOBAL_VERBOSE) {
                puts(h ? "hit" : (EvicFlag ? "miss eviction" : "miss"));
            }
        }
    }
    fclose(fp);

}











int main(int argc, char* argv[])
{
    // complete your simulator
    int s = -1, E=-1, b=-1, verbose = 0;
    const char *trace_file = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (opt) {
            case 's': s = atoi(optarg); break;
                case 'E': E = atoi(optarg); break;
                case 'b': b = atoi(optarg); break;
                case 't': trace_file = optarg; break;
                case 'v': verbose = 1; break;
                case 'h' : print_usage(argv); return 0;
                default: print_usage(argv); return 1;;
        }
    }

    // output cache hit and miss statistics

    if (s<0 || E<=0 || b<0 || trace_file==NULL) {
        print_usage(argv); return 1;
    }

    GLOBAL_VERBOSE = verbose;

    Cache MoneyMoney = make_cache(s, E, b);


    (void)GLOBAL_VERBOSE;

    do_trace(&MoneyMoney, trace_file);


    // testing to make sure the teachers code works and prints correctly


     print_summary(Global_hits, Global_misses, Global_evicts);
    free_da_cache(&MoneyMoney);
    // assignment done. life is good!
    return 0;
}
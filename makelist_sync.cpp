#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define KEYLEN 3
#define ACHUNK 10000

struct VMElistStruct {
	int event;	// events in file counter
	int cycle;	// cycles in file counter
	int hcycle;	// hardware cycle counter
	int cnt;	// events in the cycle
};

struct PRPlistStruct {
	int cycle;	// cycles in file counter
	int hcycle;	// hardware cycle counter
	int cnt;	// events in the cycle
};

struct VMElistStruct *VMEarray = NULL;
struct PRPlistStruct *PRParray = NULL;
int VMEsize = 0;
int PRPsize = 0;
int VMElen = 0;
int PRPlen = 0;

int ReadVME(FILE *fIn)
{
	char str[1024];
	char *ptr;
	int cnt;
	
	cnt = 0;
	for (;;) {
		ptr = fgets(str, sizeof(str), fIn);
		if (!ptr) break;
		if (VMEsize < cnt+1) {
			VMEsize += ACHUNK;
			VMEarray = (struct VMElistStruct *) realloc(VMEarray, VMEsize * sizeof(struct VMElistStruct));
			if (!VMEarray) {
				printf("No memory !\n");
				return -1;
			}
		}
		sscanf(str, "%d %d %d %d", &VMEarray[cnt].event, 
			&VMEarray[cnt].cycle, &VMEarray[cnt].hcycle, &VMEarray[cnt].cnt);
//		printf("%d: %s => %d %d %d %d\n", cnt, str, VMEarray[cnt].event, 
//			VMEarray[cnt].cycle, VMEarray[cnt].hcycle, VMEarray[cnt].cnt);
		cnt++;
	}
	return cnt;
}

int ReadPRP(FILE *fIn)
{
	char str[1024];
	char *ptr;
	int cnt;
	
	cnt = 0;
	for (;;) {
		ptr = fgets(str, sizeof(str), fIn);
		if (!ptr) break;
		if (PRPsize < cnt+1) {
			PRPsize += ACHUNK;
			PRParray = (struct PRPlistStruct *) realloc(PRParray, PRPsize * sizeof(struct PRPlistStruct));
			if (!PRParray) {
				printf("No memory !\n");
				return -1;
			}
		}
		sscanf(str, "%d %d %d", &PRParray[cnt].cycle, &PRParray[cnt].hcycle, &PRParray[cnt].cnt); 
		cnt++;
	}
	return cnt;
}

int MakeVMEkey(int ptr, int *key)
{
	int i;
	for (;;) {
		if (ptr >= VMElen - KEYLEN) return -1;
		for (i=1; i<KEYLEN; i++) if (VMEarray[ptr+i-1].hcycle + 1 != VMEarray[ptr+i].hcycle) break;
//		printf("Ptr = %d     i=%d: %d %d\n", ptr, i, VMEarray[ptr+i-1], VMEarray[ptr+i]);
		if (i != KEYLEN) {
			ptr += i;
			continue;
		}
		for (i=0; i<KEYLEN; i++) key[i] = VMEarray[ptr+i].cnt;
		return ptr;
	}
	return -1;
}

int FindPRPKey(int ptr, int *key)
{
	int i;
	for (;ptr < PRPlen - KEYLEN; ptr++) {
		for (i=0; i<KEYLEN; i++) if (PRParray[ptr+i].cnt != key[i]) break;
		if (i != KEYLEN) continue;
		for (i=1; i<KEYLEN; i++) if (PRParray[ptr+i-1].hcycle + 1 != PRParray[ptr+i].hcycle) break;
		if (i != KEYLEN) continue;
		return ptr;
	}
	return -1;
}

int CheckNext(int VMEptr, int PRPptr)
{
	if (VMEptr + 1 >= VMElen || PRPptr + 1 >= PRPlen) return false;
	if (VMEarray[VMEptr+1].cnt != PRParray[PRPptr+1].cnt) return false;
	if (VMEarray[VMEptr+1].hcycle != VMEarray[VMEptr].hcycle + 1) return false;
	if (PRParray[PRPptr+1].hcycle != PRParray[PRPptr].hcycle + 1) return false;
	return true;
}

int main(int argc, char **argv)
{
	int irc, fnum;
	FILE *fVME;
	FILE *fPRP;
	FILE *fOut;
	int VMEptr;
	int PRPptr;
	int key[KEYLEN];
	char str[1024];
	int i;
	
	if (argc < 2) {
		printf("Merge DANSS and EPECUR data files with the same run number\n");
		printf("Part II1 - make a synchronization list\n");
		printf("Usage: %s NNN\n", argv[0]);
		printf("Will use input files: data/prop_list_NNN.data data/vme_list_NNN.data\n");
		printf("And output file data/sync_list_NNN.data\n");
		return 0;
	}

	fnum = strtol(argv[1], NULL, 10);

	sprintf(str, "data/vme_list_%3.3d.data", fnum);
	fVME = fopen(str, "rt");

	sprintf(str, "data/prop_list_%3.3d.data", fnum);
	fPRP = fopen(str, "rt");

	sprintf(str, "data/sync_list_%3.3d.data", fnum);
	fOut = fopen(str, "wt");

	if (!fVME || !fPRP || !fOut) {
		printf("Can not open all required files\n");
		return 10;
	}

	VMElen = ReadVME(fVME);
	PRPlen = ReadPRP(fPRP);
	if (VMElen <= 0 || PRPlen <= 0) {
		printf("Nothing to do !\n");
		goto fin;
	}
	printf("Arrays read: %d (VME) and %d (PRP)\n", VMElen, PRPlen);
	
	VMEptr = 0;
	PRPptr = 0;
	for (;;) {
		VMEptr = MakeVMEkey(VMEptr, key);
		if (VMEptr < 0) break;
		irc = FindPRPKey(PRPptr, key);
		if (irc < 0) {
			VMEptr++;
			continue;
		} 
		PRPptr = irc;
		fprintf(fOut, "%8d %8d %6d\n", PRParray[PRPptr].cycle, 
			VMEarray[VMEptr].event - VMEarray[VMEptr].cnt, VMEarray[VMEptr].cnt); 
		for (i=0;;i++) if (CheckNext(VMEptr+i, PRPptr+i)) {
			fprintf(fOut, "%8d %8d %6d\n", PRParray[PRPptr+i+1].cycle, 
				VMEarray[VMEptr+i+1].event - VMEarray[VMEptr+i+1].cnt, VMEarray[VMEptr+i+1].cnt); 
		} else {
			VMEptr += i+1;
			PRPptr += i+1;
			break;
		}
	}

fin:
	if (VMEarray) free(VMEarray);
	if (PRParray) free(PRParray);
	if (fVME) fclose(fVME);
	if (fPRP) fclose(fPRP);
	if (fOut) fclose(fOut);
	return 0;
}

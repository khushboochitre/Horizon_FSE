#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <pwd.h>

struct AARecInfo {
	unsigned long long Diff:48;
	unsigned long long Alias:1;
	unsigned long long Invalid:15;
};

#define MAX_LEN 1000
struct FuncInfo {
	int MaxId;
	char *Base;
	int FID; 
	char Name[MAX_LEN];
	int NumArgs; 
	int NumDefs;
};

struct FuncInfo* FuncMap = NULL;
static int IdsInFuncMap = 0;
static FILE *fptr_d = NULL;
static FILE *fptr_d1 = NULL;
static char LogDir[MAX_LEN] = {'\0'};

static void setLogDir() {
	if (!LogDir[0]) {
	#if 0
		int len;
		const char *LogDirEnv = getenv("LOG_DIR");
    if (!LogDirEnv) {
   		LogDirEnv = getenv("HOME");
    	assert(LogDirEnv);
			len = snprintf(LogDir, MAX_LEN, "%s/logs", LogDirEnv);
		} 
		else {
			len = snprintf(LogDir, MAX_LEN, "%s", LogDirEnv);
		}
	#endif
		int len;
		struct passwd *pw = getpwuid(getuid());
		const char *LogDirEnv = pw->pw_dir;
		assert(LogDirEnv);
		len = snprintf(LogDir, MAX_LEN, "%s/logs", LogDirEnv);
		assert(len < MAX_LEN);
		LogDir[len] = '\0';
		//printf("LogDir: %s\n", LogDir);
	}
}


extern "C" void getDebugInfo(char *file, unsigned FID, unsigned VID, void *Value) {
	if (fptr_d == NULL) {
    	char file1[MAX_LEN], file2[MAX_LEN];
		int len;

		setLogDir();
    	len = snprintf(file1, MAX_LEN, "%s/%s.txt", LogDir, file);
    	assert(len < MAX_LEN);
    	file1[len] = '\0';
    	fptr_d = fopen(file1, "w");
   
    	if (fptr_d == NULL) {
    	    printf("Error in opening trace file!");
    	    exit(1);
    	}

    	len = snprintf(file2, MAX_LEN, "%s/%s_pair.txt", LogDir, file);
    	assert(len < MAX_LEN);
    	file2[len] = '\0';
    	fptr_d1 = fopen(file2, "w");
   
    	if (fptr_d1 == NULL) {
    	    printf("Error in opening trace file!");
    	    exit(1);
    	}
	}
  	//fprintf(fptr_d, "%d %d %p\n", FID, VID, Value);
}

extern "C" char* getAABasePtr(int FuncId, unsigned MaxId, char *Name, int NumArgs, int NumDefs) {
	assert(FuncId >= 0);
	assert(MaxId > 0);
	if (FuncId >= IdsInFuncMap) {
		int TotalIds = FuncId + 16;
		size_t NewSz = TotalIds * sizeof(struct FuncInfo);
		size_t OldSz = 0;
		struct FuncInfo *NewMap = (struct FuncInfo*)malloc(NewSz);
		assert(NewMap);
		if (IdsInFuncMap > 0) {
			assert(FuncMap);
			OldSz = IdsInFuncMap * sizeof(struct FuncInfo);
			memcpy(NewMap, FuncMap, OldSz);
			free(FuncMap);
		}
		char *ZeroOff = ((char*)NewMap) + OldSz;
		memset(ZeroOff, 0, NewSz - OldSz);
		IdsInFuncMap = TotalIds;
		FuncMap = NewMap;
	}
	char *Ret = FuncMap[FuncId].Base;
	if (Ret == NULL) {
		size_t NumSlots = ((size_t)MaxId * (MaxId + 1)) / 2;
		size_t AAInfoSz = NumSlots * sizeof(struct AARecInfo);
		size_t i;
		struct AARecInfo *Base = (struct AARecInfo*)malloc(AAInfoSz);
		assert(Base);
		Ret = (char*)Base;
		for (i = 0; i < NumSlots; i++) {
			Base[i].Invalid = 1;
			Base[i].Alias = 1;
		}
		FuncMap[FuncId].MaxId = MaxId;
		FuncMap[FuncId].Base = Ret;
		//FuncMap[FuncId].Name = Name;
		char *FName = FuncMap[FuncId].Name;
		strncpy(FName, Name, MAX_LEN);

		FuncMap[FuncId].NumArgs = NumArgs;
		FuncMap[FuncId].NumDefs = NumDefs;
	}
	return Ret;
}

static int getOffset(int Id1, int Id2, int MaxId)
{
  int tmp;
  assert(Id1 != Id2);
  if (Id1 > Id2) {
    tmp = Id1;
    Id1 = Id2;
    Id2 = tmp;
  }

  // MaxId + (MaxId - 1) + (MaxId  -2) + ...
  int XOff = 0;
  int i;
  for (i = 0; i < Id1; i++) {
    XOff += MaxId;
    MaxId--;
  }
  int YOff = Id2 - (Id1 + 1);
  int Off = XOff + YOff;

  return Off;
}

extern "C" unsigned long long getAliasInfo() {
	return 0;
}

extern "C" void printValAddress(unsigned FID, unsigned VID1, unsigned VID2, void *Def, void *MVar, unsigned S1, unsigned S2) {
#if 0
	unsigned long long tcnt = 0, fcnt = 0;
	assert(fptr_d1);
	//fprintf(fptr_d1, "FID:%d ID1:%d ID2:%d Def:%p MVar:%p \n", FID, VID1, VID2, Def, MVar);
	if (FuncMap) {
		struct FuncInfo *Info = &FuncMap[FID];
		if (Info->Base) {
			int off = getOffset(VID1, VID2, Info->MaxId);
			struct AARecInfo *Rec = (struct AARecInfo*)Info->Base;
			tcnt = (unsigned long long)Rec[off].TrueCount;
			fcnt = (unsigned long long)Rec[off].FalseCount;
		}
	}
	int tmp;
	void *tmp1;
	if (VID2 < VID1) {
		tmp = VID2;
		VID2 = VID1;
		VID1 = tmp;
		tmp1 = Def;
		Def = MVar;
		MVar = tmp1;
		tmp = S1;
		S1 = S2;
		S2 = tmp;
	}
	fprintf(fptr_d1, "FID:%d V1:%d V2:%d P1:%p P2:%p C1:%llu C2:%llu S1:%d S2:%d\n", FID, VID1, VID2, Def, MVar, tcnt, fcnt, S1, S2);
#endif
}

extern "C" void ReadFromFile(char *file1) {
	if(access(file1, F_OK) == -1) {
		return;
	}

  FILE *fptr = fopen(file1, "r");
  if (fptr == NULL) {
      printf("Error in opening log file!");
      exit(1);
  }

  char *line = NULL;
	size_t flen = 0;
	ssize_t read;
 
	while ((read = getline(&line, &flen, fptr)) != -1) {
		int rec = 0;
		int FID, VID1, VID2;
		unsigned long long Count1, Count2;
		char *Name;
		int NumArgs, NumDefs, MaxId;
		struct AARecInfo *Rec = NULL;

		for (char *p = strtok(line, " "); p != NULL; p = strtok(NULL, " ")) {
			switch (rec) {
				case 0:
      			FID = atoi(p);
						break;
				case 1:
						Name = p;
						break;
				case 2:
						NumArgs = atoi(p);
						break;
				case 3:
						NumDefs = atoi(p);
						break;
				case 4:
						MaxId = atoi(p);
						//printf("FID: %d Name:%s NumArgs:%d MaxId:%d NumDefs:%d\n", FID, Name, NumArgs, MaxId, NumDefs);
						Rec = (struct AARecInfo*)getAABasePtr(FID, MaxId, Name, NumArgs, NumDefs);
						assert(Rec != NULL);
						break;
				case 5:
      			VID1 = atoi(p);
						break;
				case 6:
      			VID2 = atoi(p);
						break;
				case 7:
      			Count1 = strtoll(p, NULL, 10);
						break;
				case 8:
      			Count2 = strtoll(p, NULL, 10);
      			int off = getOffset(VID1, VID2, MaxId);
						struct AARecInfo *Cur = (struct AARecInfo*)&Count1;

						assert(Count2 == 0);
						if (Rec[off].Invalid) {
							Rec[off].Alias = Cur->Alias;
							Rec[off].Diff = Cur->Diff;
							Rec[off].Invalid = Cur->Invalid;
						}
						else if (!Cur->Invalid) {
							Rec[off].Alias &= Cur->Alias;
							if (Cur->Diff < Rec[off].Diff) {
								Rec[off].Diff = Cur->Diff;
							}
						}
						rec = 4;
						break;
			}
      rec++;
		}
	}
	fclose(fptr);
}

extern "C" void isMustPair(int FID, int ID1, int ID2, void *P1, void *P2) {
	if(P1 != P2) {
		assert(fptr_d);
		fprintf(fptr_d, "Failing MustAlias query FID:%d ID1:%d ID2:%d P1:%p P2:%p\n", FID, ID1, ID2, P1, P2);
		fsync(fileno(fptr_d));
		fclose(fptr_d);
		assert(0);
		exit(0);
	}
}

extern "C" void isNoPair(int FID, int ID1, int ID2, void *P1, void *P2, unsigned long long S1, unsigned long long S2) {
	if(P1 == P2) {
		assert(fptr_d);
		fprintf(fptr_d, "Failing NoAlias query FID:%d ID1:%d ID2:%d P1:%p P2:%p\n", FID, ID1, ID2, P1, P2);
		fsync(fileno(fptr_d));
		fclose(fptr_d);
		assert(0);
		exit(0);
	}
	else if((P1 < P2 && ((((char *)P2 - (char *) P1) < S1) || (((char *)P2 - (char *) P1) < S2))) || (P2 < P1 && ((((char *)P1 - (char *) P2) < S1) || (((char *)P1 - (char *) P2) < S2)))) {
		assert(fptr_d);
		fprintf(fptr_d, "Failing PartialAlias query FID:%d ID1:%d ID2:%d P1:%p P2:%p S1:%llu S2:%llu\n", FID, ID1, ID2, P1, P2, S1, S2);
		fsync(fileno(fptr_d));
		fclose(fptr_d);
		assert(0);
		exit(0);
	}
}

static void synchronize(char *path, int lock) {
	static FILE *LockFile = NULL;
	int ret;
	if (lock == 0) {
		assert(LockFile);
		ret = flock(fileno(LockFile), LOCK_UN);
		assert(ret != -1);
		fclose(LockFile);
		LockFile = NULL;
	}
	else {
		assert(!LockFile);
		char Name[128];
		int len;
		len = snprintf(Name, 128, "%s/lock", path);
		assert(len < 128);
		LockFile = fopen(Name, "w");
		assert(LockFile);
		ret = flock(fileno(LockFile), LOCK_EX);
		assert(ret != -1);
	}
}

//<Function ID, Map<Pointer ID, Pointer ID, Alias Count, No Alias Count> >
extern "C" void WriteToFile(char *file) {
    FILE *fptr, *fptr1;
    char file1[MAX_LEN], file2[MAX_LEN];
		int len;

		setLogDir();
		synchronize(LogDir, 1);

    len = snprintf(file1, MAX_LEN, "%s/%s.txt", LogDir, file);
    assert(len < MAX_LEN);
    file1[len] = '\0';

		ReadFromFile(file1);

    fptr = fopen(file1, "w");
    if (fptr == NULL) {
        printf("Error in opening log file!");
        exit(1);
    }

    len = snprintf(file2, MAX_LEN, "%s/%s_human.txt", LogDir, file);
    assert(len < MAX_LEN);
    file2[len] = '\0';

    fptr1 = fopen(file2, "w");
    if (fptr1 == NULL) {
        printf("Error in opening log file!");
        exit(1);
    }

		int i;
	
		for (i = 0; i < IdsInFuncMap; i++) {
			struct FuncInfo *Info = &FuncMap[i];
			if (Info->Base) {
				int MaxId = Info->MaxId;
				struct AARecInfo *Rec = (struct AARecInfo*)Info->Base;
				int j, k;
				fprintf(fptr, "%d %s %d %d %d ", i, Info->Name, Info->NumArgs, Info->NumDefs, Info->MaxId);

				fprintf(fptr1, "Funtion: ID:%d %s %d %d %d\n", i, Info->Name, Info->NumArgs, Info->NumDefs, Info->MaxId);
				for (j = 0; j < MaxId; j++) {
					for (k = j + 1; k < MaxId; k++) {
						int off = getOffset(j, k, MaxId);
						unsigned long long TrueCount = *((unsigned long long*)&Rec[off]);
						unsigned long long FalseCount = 0;
						fprintf(fptr1, "\tID1:%d ID2:%d TRUE:%llu FALSE:%llu\n", 
							j, k, TrueCount, FalseCount);
						if (!Rec[off].Invalid) {
							fprintf(fptr, "%d %d %llu %llu ", j, k, TrueCount, FalseCount);
						}
					}
				}
				fprintf(fptr, "\n");
			}
		}

		fclose(fptr);
		fclose(fptr1);
		if (fptr_d) {
  			//fprintf(fptr_d, "WriteToFile\n");
			fclose(fptr_d);
		}
		synchronize(LogDir, 0);
#if 0
	string LogDir;
	if (const char *LogDirEnv = getenv("LOG_DIR")) {
	    LogDir = LogDirEnv;
	} 
	else {
	    cout << "$LOG_DIR not set.\n";
	}

	string DynamicAAFile = LogDir + "/dynamicaa.txt";
  	ofstream File(DynamicAAFile);

	map<unsigned, map<IDPair, IDPair>>::iterator itr1;
	for(itr1 = Aliases.begin(); itr1 != Aliases.end(); ++itr1) {
		unsigned FID = itr1->first;
		File << FID << " ";
		map<IDPair, IDPair> AliasPairs = itr1->second;
		map<IDPair, IDPair>::iterator itr2;
		for(itr2 = AliasPairs.begin(); itr2 != AliasPairs.end(); ++itr2) {
			File << itr2->first.first << " " << itr2->first.second << " " 
			<< itr2->second.first << " " << itr2->second.second << " ";
		}
		File << "\n";
	}
	File.close();
#endif
}


#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<stdint.h>
#include<errno.h>
#include<string.h>

#define ROOTINO (1)  // root i-number
#define BSIZE (512)  // block size

#define NDIRECT (12)


// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
};

void *orig_ptr;
void *ptr;
void* data_block;
char* my_bit_map;
int* block_count;
int* ref_count;
int* dir_count;

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          
  short minor;          
  short nlink;          // Number of links to inode
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

void print_usage() {
	fprintf(stderr,"Usage: xv6_fsck <file_system_image>\n");
        exit(1);
}

void print_not_found() {
	fprintf(stderr,"image not found.\n");
	exit(1);
}

void balloc(uint num) {
	my_bit_map[num/8] = my_bit_map[num/8] | (0x1 << (num%8));
}

void check_address_valid(struct dinode *p, uint start, uint end, void* bit_map) {
	for(int i = 0; i < 13; i++) {
		uint addr = p->addrs[i];
		if(addr!=0) {
			if(addr<start || addr>end) {
				if(i==NDIRECT) {
					fprintf(stderr,"ERROR: bad indirect address in inode.\n");
				} else {
					fprintf(stderr,"ERROR: bad direct address in inode.\n");
				}
				exit(1);
			}
			char ch = *((char *)(bit_map + (addr / 8)));
			if(!((ch >> (addr % 8)) & 0x01)) {
				fprintf(stderr,"ERROR: address used by inode but marked free in bitmap.\n");
				exit(1);
			}
			balloc(addr);
			if(block_count[addr]!=0) {
				fprintf(stderr,"ERROR: direct address used more than once.\n");
				exit(1);
			}
			block_count[addr]=1;
			if(i==NDIRECT) {
				int nind = p->size/BSIZE - NDIRECT;
				if(p->size%BSIZE != 0)
					nind++;
				if(nind > 0) {
					int *ind = orig_ptr + (addr * BSIZE);
					for(int j = 0; j < nind; j++) {
						uint inaddr = *ind;
						if(inaddr<start || inaddr > end) {
							fprintf(stderr,"ERROR: bad indirect address in inode.\n");
							exit(1);
						}
						char c = *((char *)(bit_map + (inaddr / 8)));
						if(!((c >> (inaddr % 8)) & 0x01)) {
							fprintf(stderr,"ERROR: address used by inode but marked free in bitmap.\n");
							exit(1);
						}
						balloc(inaddr);
						if(block_count[inaddr]!=0) {
							fprintf(stderr,"ERROR: indirect address used more than once.\n");
							exit(1);
						}
						block_count[inaddr]=1;
						ind++;
					}
					
				}
			}
		}
	}
}

void check_spl_case(int parent_inum, int child_inum) {
	struct dinode *p = (struct dinode*)(ptr + (parent_inum * sizeof(struct dinode)));
	int nd = BSIZE/(sizeof(struct dirent));
	int nb = p->size/BSIZE;
	if(p->size%BSIZE)
		nb++;
	int flag = 1;
	for(int k = 0; k < 13; k++) {
		struct dirent *d = orig_ptr + ( p->addrs[k] * BSIZE );
		if(p->addrs[k]==0)
			continue;
		if(k==NDIRECT) {
			nb -= NDIRECT;
			int *indb = (int *)d;	
			for(int l=0;l<nb;l++) {
				struct dirent *yy = orig_ptr + ( indb[l] * BSIZE);
				for(int j = 0;j < nd; j++) {
					if(yy->inum!=0) {
						if(!strcmp(yy->name,".")) {
							if(!strcmp(yy->name,"..")) {
								if(yy->inum==child_inum) {
									flag = 0;
								}
							}
						}
					}
					yy++;
				}	
			}
		}
		for(int j = 0;j < nd; j++ ) {
			if(d->inum!=0) {
				if(d->inum == child_inum)
					flag = 0;
			}
			d++;
		}		
	}
	if(flag==1) {
		fprintf(stderr,"ERROR: parent directory mismatch.\n");
		exit(1);
	}
}

void check_dir_fmt(struct dinode *p, int inode) {
			struct dirent *d = orig_ptr + ( p->addrs[0] * BSIZE );
			if(strcmp(d->name,".")||strcmp((d+1)->name,"..")) {
				fprintf(stderr,"ERROR: directory not properly formatted.\n");
				exit(1);
			}
			check_spl_case(d->inum,inode);
}

void check_bit_map(char* bit_map) {
	for(int i = 0; i < BSIZE;i++) {
		if(my_bit_map[i] != bit_map[i]) {
			fprintf(stderr,"ERROR: bitmap marks block in use but it is not in use.\n");
			exit(1);
		}
	}
}

void check_inode_map(int* map, int num, int count) {
	int array[num];
	memcpy(array,map,sizeof(array));
	for(int i=0;i<num;i++) {
		struct dinode *p = (struct dinode*)(ptr + (i * sizeof(struct dinode)));
		if(p->type  == 0) {
			continue;
		} else if(p->type==1) {
			int nd = BSIZE/(sizeof(struct dirent));
			int nb = p->size/BSIZE;
			if(p->size%BSIZE)
				nb++;
			for(int k = 0; k < 13; k++) {
				struct dirent *d = orig_ptr + ( p->addrs[k] * BSIZE );
				if(k==NDIRECT) {
					nb -= NDIRECT;
					int *indb = (int *)d;	
					for(int l=0;l<nb;l++) {
						struct dirent *yy = orig_ptr + ( indb[l] * BSIZE);
						for(int j = 0;j < nd; j++) {
							if(yy->inum!=0) {
								array[yy->inum] = 0;
								struct dinode *pp = (struct dinode*)(ptr + ((yy->inum) * sizeof(struct dinode)));
								if(pp->type == 2) {
									ref_count[yy->inum]--;
								} else if(pp->type == 1) {
									if(strcmp(d->name,"."))
										if(strcmp(d->name,".."))
											dir_count[yy->inum]++;
								}
							}
							yy++;
						}	
					}
				}
				for(int j = 0;j < nd; j++ ) {
					if(d->inum!=0) {
						array[d->inum] = 0;
						if(map[d->inum]!=1) {
							fprintf(stderr,"ERROR: inode referred to in directory but marked free.\n");
							exit(1);
						}
						struct dinode *pp = (struct dinode*)(ptr + ((d->inum) * sizeof(struct dinode)));
						if(pp->type == 2) {
							ref_count[d->inum]--;
						} else if(pp->type ==1) {
							if(strcmp(d->name,"."))
								if(strcmp(d->name,".."))
									dir_count[d->inum]++;
						}
					}
					d++;
				}		
			}
		}
	}

	for(int i=0;i<num;i++) {
		if(array[i]==1) {
			fprintf(stderr,"ERROR: inode marked use but not found in a directory.\n");
			exit(1);
		}
		if(ref_count[i]>0) {
			fprintf(stderr,"ERROR: bad reference count for file.\n");
			exit(1);
		}
		if(dir_count[i]>2) {
			fprintf(stderr,"ERROR: directory appears more than once in file system.\n");
			exit(1);
		}
	}	
} 

int main(int argc, char* argv[]) {
	int repair_flag = 0;
	char c;
	char *file_s = NULL;
	
	//Obtain the input and output file info
        while((c=getopt(argc,argv,":r:"))!=-1) {
                switch(c) {
                        case 'r': repair_flag = 1;
				  file_s = optarg;
                                  break;
                        default: print_usage();
                }
        }
        
    	if(repair_flag == 0) {
		if(argc==2)
			file_s = argv[optind];
		else 
			print_usage();
	} else {
		if(argc!=3)
			print_usage();
	}
	
	int fd = open(file_s,O_RDONLY);	
	if(fd < 0)
		print_not_found();
	
	
	struct stat img_stat;
	int ret_val = fstat(fd,&img_stat);
	if(ret_val!=0)
		exit(1);
	
	orig_ptr = mmap(NULL, img_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(orig_ptr == MAP_FAILED) 
		exit(1);  
	
	struct superblock *sb = (struct superblock *) (orig_ptr + BSIZE);
	//printf("Size:%d\nNo of blocks:%d\nNo of inodes:%d\n",sb->size,sb->nblocks,sb->ninodes);
	//printf("Size of inode:%ld\n",sizeof(struct dinode));
	char array[BSIZE];
	memset(array, 0, BSIZE);
	int array1[sb->nblocks];
	memset(array1, 0,sizeof(array1));
	int array2[sb->ninodes];
	memset(array2, 0,sizeof(array2));
	int array3[sb->ninodes];
	memset(array3, 0,sizeof(array3));
	int array4[sb->ninodes];
	memset(array4, 0,sizeof(array4));
	dir_count = array4;
	ref_count = array3;	
	block_count = array1;
	my_bit_map = array;
	int used = ((sb->ninodes * sizeof(struct dinode))/BSIZE) + 3;
	for(int k = 0; k <= used;k++) {
		balloc(k);
	}
	ptr = orig_ptr + (2*BSIZE);
	void* bit_map = orig_ptr + (3 * BSIZE) + (sb->ninodes * sizeof(struct dinode));
	data_block = bit_map + BSIZE;
	int count = 0;
	for(int i = 0; i < sb->ninodes; i++) {
		struct dinode *p = (struct dinode*)(ptr + (i * sizeof(struct dinode)));
		//printf("Inode:%d->%d\n",i+1,p->type);
		if(i==1) {
			if( p->type!=1 ) {
				fprintf(stderr,"ERROR: root directory does not exist.\n");
				exit(1);
			}
			check_dir_fmt(p,i); 
			struct dirent *d = orig_ptr + ( p->addrs[0] * BSIZE );
			if(d->inum!=1 || (d+1)->inum!=1) {
				fprintf(stderr,"ERROR: root directory does not exist.\n");
				exit(1);
			}
		}
		if(p->type  == 0) {
			continue;
		} else if(p->type > 3) {
			fprintf(stderr,"ERROR: bad inode.\n");
			exit(1);
		} else {
			check_address_valid(p,((bit_map-orig_ptr)/BSIZE),sb->size, bit_map);
			if(p->type == 1) {
				check_dir_fmt(p,i);
				array4[i] = 1;
			}
			if(p->type == 2) 
				array3[i] = p->nlink;
		}
		array2[i] = 1;
		count++;
	}
	check_bit_map((char*)bit_map);
	check_inode_map(array2,sb->ninodes,count);
}	

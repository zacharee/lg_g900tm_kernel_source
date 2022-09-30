#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/vfs.h>
#include <ctype.h>

#include "dump_exfat.h"

char partition_path[256] = {0,};
char fs_name[9] = {0,};
long long vol_length=0;
int fat_offset=0;
int fat_length=0; // byte
int sector_size=0; // byte
int cluster_size=0; // byte
int cluster_heap_offset=0;
int cluster_count=0;
int first_cluster_of_root_dir=0;
char volume_flags[2] = {0,};
int number_of_fat=0;
int percent_in_use=0;

FILE *dev_fd = NULL; // Use device fd as global, otherwise I need to open many time
char *fat_table = NULL; // FAT Table is needed for showing, file lookup and so on. Just read at once.

void hex_print(char *buf, int length)
{
    long long pos = 0;
    int i = 0;
    char to_str[17] = {0,};
    int skip = 0;

    for(pos=0; pos<length; pos=pos+0x10) {
        for(i=0; i<0x10; i++) {
            if(buf[pos+i]) {
                skip = 0;
                break;
            }
            if(i == 0x0F) {
                if(!skip)
                    printf("***\n");
                skip=1;
            }
        }
        if (!skip) {
            printf("%08llX: ", pos);
            for(i=0; i<0x10; i++) {
                printf("%02X", buf[pos+i]);
                to_str[i] = ((buf[pos+i]>=' ' && buf[pos+i]<='~') ? buf[pos+i] : '.');
                if(i%2 == 1)
                    printf(" ");
            }
            printf(" %s\n", to_str);
        }
    }
}

void exfat_statfs(void)
{
    FILE *proc_fd;
    char buf[512] = {0,};
    char *str;
    struct statfs es; // exfat_statfs
    char mnt_pnt[128] = {0,};
    int found = 0;
    int i = 0;

    if (!(proc_fd = fopen(MOUNTS_INFO, "r"))) {
        printf("Failed to open %s. err:%s(%d)\n", MOUNTS_INFO, strerror(errno), errno);
        exit(-1);
    }

    printf("----------------------------- statfs info ----------------------------\n");
    while(fgets(buf, sizeof(buf), proc_fd)) {
        if(strstr(buf, "exfat") && (strstr(buf, partition_path) || strstr(buf, "media_rw"))) {
            str = strtok(buf, " ");
            while (str != NULL) {
                if (i == 1) {   // index 1 means mount point in /proc/mounts
                    strcpy(mnt_pnt, str);
                    printf("Device Mount Point\t\t\t: %s\n", mnt_pnt);
                    found = 1;
                    break;
                }
                str = strtok(NULL, " ");
                i++;
            }
        }
    }
    fclose(proc_fd);

    if (!found) {
        printf("There is no mounted exfat filesystem currently.\n");
    } else {
        if (statfs(mnt_pnt, &es))
            printf("err:%s(%d)\n", strerror(errno), errno);

#if defined(__LP64__)
        /*Free Cluster count*/
        printf("Free Cluster Count\t\t\t: %lu\n", es.f_bfree);

        /*Available Cluster count*/
        printf("Available Cluster Count\t\t\t: %lu\n", es.f_bavail);

        /*Max File name length*/
        printf("Max File Name Length\t\t\t: %lu\n", es.f_namelen);

        /*Mount flags*/
        printf("Mount flags\t\t\t\t: 0x%08lX\n", es.f_flags);
#else
        /*Free Cluster count*/
        printf("Free Cluster Count\t\t\t: %llu\n", es.f_bfree);

        /*Available Cluster count*/
        printf("Available Cluster Count\t\t\t: %llu\n", es.f_bavail);

        /*Max File name length*/
        printf("Max File Name Length\t\t\t: %u\n", es.f_namelen);

        /*Mount flags*/
        printf("Mount flags\t\t\t\t: 0x%08X\n", es.f_flags);
#endif
    }
}

void meta_print(char *buf)
{
    printf("Field Name\t\t\t\tOffset\tSize\tValue\n");

    /*BytesPerSectorShift - check sector size for other calculation*/
    memcpy(&sector_size, buf+BYTE_PER_SECTOR_SHIFT_OFFSET, BYTE_PER_SECTOR_SHIFT_LENGTH);
    printf("BytesPerSectorShift\t\t\t%Xh\t%d\t%d (%d Byte)\n", BYTE_PER_SECTOR_SHIFT_OFFSET, BYTE_PER_SECTOR_SHIFT_LENGTH, sector_size, 1<<sector_size);
    sector_size = 1<<sector_size;

    /*SectorsPerClustersShift - check cluster size for other calculation*/
    memcpy(&cluster_size, buf+SECTOR_PER_CLUSTER_SHIFT_OFFSET, SECTOR_PER_CLUSTER_SHIFT_LENGTH);
    printf("SectorsPerClusterShift\t\t\t%Xh\t%d\t%d (%d KB)\n", SECTOR_PER_CLUSTER_SHIFT_OFFSET, SECTOR_PER_CLUSTER_SHIFT_LENGTH, cluster_size, (1<<cluster_size)*sector_size/1024);
    cluster_size = (1<<cluster_size)*sector_size;

    /*FileSystemName*/
    memcpy(fs_name, buf+FS_NAME_OFFSET, FS_NAME_LENGTH);
    printf("FileSystemName\t\t\t\t%Xh\t%d\t%s\n", FS_NAME_OFFSET, FS_NAME_LENGTH, fs_name);

    /*VolumeLength*/
    memcpy(&vol_length, buf+VOLUME_LENGTH_OFFSET, VOLUME_LENGTH_LENGTH);
    if((vol_length*sector_size)/(1024*1024*1024)>1)
        printf("VolumeLength\t\t\t\t%Xh\t%d\t%lld sectors (= %.2f GB)\n", VOLUME_LENGTH_OFFSET, VOLUME_LENGTH_LENGTH, vol_length, ((double)vol_length*sector_size)/(1024*1024*1024));
    else
        printf("VolumeLength\t\t\t\t%Xh\t%d\t%lld sectors (= %.2f MB)\n", VOLUME_LENGTH_OFFSET, VOLUME_LENGTH_LENGTH, vol_length, ((double)vol_length*sector_size)/(1024*1024));

    /*FatOffset*/
    memcpy(&fat_offset, buf+FAT_OFFSET, FAT_LENGTH);
    fat_offset = fat_offset*sector_size;
    printf("FatOffset\t\t\t\t%Xh\t%d\t0x%X Byte\n", FAT_OFFSET, FAT_LENGTH, fat_offset);

    /*FatLength*/
    memcpy(&fat_length, buf+FAT_LENGTH_OFFSET, FAT_LENGTH_LENGTH);
    printf("FatLength\t\t\t\t%Xh\t%d\t%d Byte\n", FAT_LENGTH_OFFSET, FAT_LENGTH, fat_length*sector_size);
    fat_length = fat_length*sector_size;

    /*ClusterHeapOffset*/
    memcpy(&cluster_heap_offset, buf+CLUSTER_HEAP_OFFSET_OFFSET, CLUSTER_HEAP_OFFSET_LENGTH);
    printf("ClusterHeapOffset\t\t\t%Xh\t%d\t0x%X Byte\n", CLUSTER_HEAP_OFFSET_OFFSET, CLUSTER_HEAP_OFFSET_LENGTH, cluster_heap_offset*sector_size);

    /*ClusterCount*/
    memcpy(&cluster_count, buf+CLUSTER_COUNT_OFFSET, CLUSTER_COUNT_LENGTH);
    printf("ClusterCount\t\t\t\t%Xh\t%d\t%d\n", CLUSTER_COUNT_OFFSET, CLUSTER_COUNT_LENGTH, cluster_count);

    /*FirstClusterOfRootDirectory*/
    memcpy(&first_cluster_of_root_dir, buf+FIRST_CLUSTER_OF_ROOT_DIR_OFFSET, FIRST_CLUSTER_OF_ROOT_DIR_LENGTH);
    printf("FirstClusterOfRootDirectory\t\t%Xh\t%d\t%d\n", FIRST_CLUSTER_OF_ROOT_DIR_OFFSET, FIRST_CLUSTER_OF_ROOT_DIR_LENGTH, first_cluster_of_root_dir);
    printf("FirstClusterOfRootDirectory(Address)\t%Xh\t%d\t0x%X Byte\n", FIRST_CLUSTER_OF_ROOT_DIR_OFFSET, FIRST_CLUSTER_OF_ROOT_DIR_LENGTH, (cluster_heap_offset*sector_size)+(first_cluster_of_root_dir-2)*cluster_size);

    /*VolumeFlags*/
    memcpy(volume_flags, buf+VOLUME_FLAG_OFFSET, VOLUME_FLAG_LENGTH);
    printf("VolumeFlags\t\t\t\t%Xh\t%d\t0x%02X%02X\n", VOLUME_FLAG_OFFSET, VOLUME_FLAG_LENGTH, volume_flags[1], volume_flags[0]);

    /*NumberOfFAT*/
    memcpy(&number_of_fat, buf+NUM_OF_FAT_OFFSET, NUM_OF_FAT_LENGTH);
    printf("NumberOfFats\t\t\t\t%Xh\t%d\t%d\n", NUM_OF_FAT_OFFSET, NUM_OF_FAT_LENGTH, number_of_fat);

    /* Get another information from vfs statfs */
    exfat_statfs();
}

// NameHash function is not modified and come from MS exfat spec site.
UInt16 NameHash(
        WCHAR * FileName,    // points to an in-memory copy of the up-cased file name
        UCHAR   NameLength
        )
{
    UCHAR  * Buffer = (UCHAR *)FileName;
    UInt16   NumberOfBytes = (UInt16)NameLength * 2;
    UInt16   Hash = 0;
    UInt16   Index;

    for (Index = 0; Index < NumberOfBytes; Index++)
    {
        Hash = ((Hash&1) ? 0x8000 : 0) + (Hash>>1) + (UInt16)Buffer[Index];
    }
    return Hash;
}

void find_clusters(char generalsecondaryflags, unsigned int first_cluster, unsigned long long data_length, char *file_name, unsigned short file_attribute, int dump_file)
{
    unsigned int tot_clusters = 0;
    unsigned int current_cluster = 0;
    unsigned int next_cluster = 0;
    unsigned int offset = 0;
    char *buf = NULL;
    int count = 0;
    FILE *out_fd = NULL;
    char out_file_name[16+MAX_NAME_LENGTH+1] = {0,};

    if ((file_attribute & ATTR_ARCHIVE) && dump_file) {
        strncat(out_file_name, "/data/local/tmp/", 16);
        strncat(out_file_name, file_name, data_length);

        if (!(out_fd = fopen(out_file_name, "wa"))) {
            printf("File open failed. ERR : %s(%d)\n", strerror(errno), errno);
            exit(-1);
        }

        buf = (char *)malloc(cluster_size);
        if (!buf) {
            printf("Memory Allocation failed\n");
            exit(-1);
        }
        memset(buf, 0, cluster_size);
    }

    if (generalsecondaryflags & 0x2) { // Contiguous data
        tot_clusters = (data_length%cluster_size ? data_length/cluster_size+1 : data_length/cluster_size);
        if (data_length)
            printf("Used Clusters\t\t: %d ~ %d (total:%d, NoFatChain:1)\n", first_cluster, first_cluster+tot_clusters-1, tot_clusters);
        else
            printf("Used Clusters\t\t: %d ~ %d (total:%d, NoFatChain:1)\n", first_cluster, first_cluster, tot_clusters);

        if ((file_attribute & ATTR_ARCHIVE) && dump_file) {
            while(count < tot_clusters) {
                fseek(dev_fd, cluster_heap_offset*sector_size+(first_cluster-2)*cluster_size+count*cluster_size, SEEK_SET);
                fread(buf, cluster_size, 1, dev_fd);
                fwrite(buf, cluster_size, 1, out_fd);
                count++;
            }
        }
    } else { // Fragmented data
        if (!fat_table)
            get_fat_table();

        printf("Used Clusters\t\t: ");
        current_cluster = first_cluster;

        while (current_cluster != EXFAT_EOF_CLUSTER) {
            offset = current_cluster*4;
            memcpy(&next_cluster, fat_table+offset, 4);
            if(next_cluster == EXFAT_EOF_CLUSTER)
                printf("%u ", current_cluster);
            else
                printf("%u, ", current_cluster);

            tot_clusters++;

            if ((file_attribute & ATTR_ARCHIVE) && dump_file) {
                fseek(dev_fd, cluster_heap_offset*sector_size+(current_cluster-2)*cluster_size, SEEK_SET);
                fread(buf, cluster_size, 1, dev_fd);
                fwrite(buf, cluster_size, 1, out_fd);
            }

            current_cluster = next_cluster;
        }
        printf("(total:%u, NoFatChain:0)\n", tot_clusters);
    }

    if ((file_attribute & ATTR_ARCHIVE) && dump_file) {
        free(buf);
        fclose(out_fd);
        truncate(out_file_name, data_length);
        printf("\nDump File Path\t\t: %s\n", out_file_name);
    }
}

void lookup_file_path(char *f_path, int f_len, int dump_file)
{
    char *str;
    char buf[512] = {0,};   // to read dentries
    unsigned long long cluster_heap_addr = cluster_heap_offset*sector_size;
    char *uniname;
    int i=0, j=0, found=0;
    unsigned short hash_tmp;
    struct dentry de = {0,};
    int sum_of_dentries = 0;
    unsigned long long next_addr = (first_cluster_of_root_dir-2)*cluster_size;
    unsigned int next_cluster = 0, current_cluster = first_cluster_of_root_dir;

    printf("Full Path\t\t: %s\nFull Path Length\t: %d\n", f_path, f_len);
    printf("======================================================================\n");

    str = strtok(f_path, "/");
    while (str != NULL) {
        printf("File\t\t\t: %s\nFile Name Length\t: %zu\n", str, strlen(str));
        uniname = (char *)malloc(strlen(str)*2);
        memset(uniname, 0, strlen(str)*2);
        for(i=0; i<strlen(str)*2; i++) {
            if(i%2)
                uniname[i] = 0;
            else
                uniname[i] = toupper(str[i/2]);
        }

        // Pass ascii string length because NameHash makes it double.
        hash_tmp = (unsigned short)NameHash((WCHAR *)uniname, (UCHAR)strlen(str));

        // dump dentry and get some info.
        j = next_addr;
        found = 0;

        if (!fat_table)
            get_fat_table();

        do {
            fseek(dev_fd, cluster_heap_addr+j, SEEK_SET);
            fread(buf, sizeof(buf), 1, dev_fd); // READ ROOT DENTRY

            i=0;
            do {
                sum_of_dentries++;
                memcpy(&de.type, buf+i, sizeof(char));
                if (de.type == EXFAT_FILE) {
                    memcpy(&de.file_attribute, buf+4+i, sizeof(unsigned short));
                } else if (de.type == EXFAT_STREAM) {
                    memcpy(&de.generalsecondaryflags, buf+1+i, sizeof(char));
                    memcpy(&de.name_len, buf+3+i, sizeof(char));
                    memcpy(&de.name_hash, buf+4+i, sizeof(unsigned short));
                    memcpy(&de.first_cluster, buf+20+i, sizeof(unsigned int));
                    memcpy(&de.data_length, buf+24+i, sizeof(unsigned long long));
                } else {
                    i+=DENTRY_SIZE; // for 1 dentry
                    continue;
                }
                if (de.name_hash == hash_tmp) {
                    found=1;
                    break;
                }
                i+=DENTRY_SIZE; // for 1 dentry
            } while (i<sizeof(buf) && (de.type != EXFAT_UNUSED));
            if (found) {
                printf("Stream Dentry Addr\t: 0x%llX Byte\nName Hash\t\t: 0x%02X\nData Size\t\t: %lld Byte\nFile Attribute\t\t: 0x%02X\n", cluster_heap_addr+j+i, de.name_hash, de.data_length, de.file_attribute);
                if (de.first_cluster) {
                    printf("1st Cluster\t\t: 0x%04X (%u)\n", de.first_cluster, de.first_cluster);
                }
                find_clusters(de.generalsecondaryflags, de.first_cluster, de.data_length, str, de.file_attribute, dump_file);

                printf("----------------------------------------------------------------------\n");
                break;
            }
            j+=sizeof(buf); // for 1 sector

            if ((j-next_addr)>=cluster_size) {
                memcpy(&next_cluster, fat_table+(current_cluster*4), 4);
                if (next_cluster == EXFAT_EOF_CLUSTER)
                    break; // No more cluster chain
                j=(next_cluster-2)*cluster_size;
                next_addr=(next_cluster-2)*cluster_size;
                current_cluster = next_cluster;
            }

        } while (de.type != EXFAT_UNUSED && !found && (sum_of_dentries < MAX_EXFAT_DENTRIES));

        if ((!de.type && !found) || (sum_of_dentries >= MAX_EXFAT_DENTRIES)) {
            printf("*** There is no such file or directory ***\n");
            break;
        }

        free(uniname);
        str = strtok(NULL, "/");
    }
}

void get_cluster_bit_map_dentry(void)
{
    struct dentry de = {0,};
    int found = 0;
    int i = 0;
    char *buf = NULL;

    buf = (char *)malloc(sector_size);
    if (!buf) {
        printf("Memory Allocation failed\n");
        exit(-1);
    }
    memset(buf, 0, sector_size);

    fseek(dev_fd, (cluster_heap_offset*sector_size)+(first_cluster_of_root_dir-2)*cluster_size, SEEK_SET);
    fread(buf, sector_size, 1, dev_fd); // READ ROOT DENTRY

    do {
        memcpy(&de.type, buf+i, sizeof(char));
        memcpy(&de.first_cluster, buf+20+i, sizeof(unsigned int));
        memcpy(&de.data_length, buf+24+i, sizeof(unsigned long long));
        if (de.type == EXFAT_BITMAP) {
            found = 1;
            printf("Cluster Allocation Bit Map Addr\t: 0x%08X\n", (cluster_heap_offset*sector_size)+(de.first_cluster-2)*cluster_size);
            printf("Cluster Allocation Bit Map Size\t: %lld byte\n", de.data_length);
            free(buf);
            break;
        }
        i+=DENTRY_SIZE;
    } while (de.type != EXFAT_UNUSED && !found);

    buf = (char *)malloc(de.data_length);
    if (!buf) {
        printf("Memory Allocation failed\n");
        exit(-1);
    }
    memset(buf, 0, de.data_length);

    fseek(dev_fd, (cluster_heap_offset*sector_size)+(de.first_cluster-2)*cluster_size, SEEK_SET);
    fread(buf, de.data_length, 1, dev_fd);  // READ CLUSTER BIT MAP DENTRY
    hex_print(buf, de.data_length);

    free(buf);
}

void get_fat_table(void)
{
    if (fat_table)
        return ;
    fat_table = (char *)malloc(fat_length);
    if (!fat_table) {
        printf("Memory Allocation failed\n");
        exit(-1);
    }
    memset(fat_table, 0, fat_length);

    fseek(dev_fd, fat_offset, SEEK_SET);
    fread(fat_table, fat_length, 1, dev_fd);  // READ FAT TABLE
}

void start_prog(void)
{
    if (!(dev_fd = fopen(partition_path, "rb"))) {
        printf("Failed to open %s : %s(%d)\n", partition_path, strerror(errno), errno);
        exit(-1);
    }
}

void finish_prog(void)
{
    if (dev_fd)
        fclose(dev_fd);
    if (fat_table)
        free(fat_table);
}

static void usage(const char *progname)
{
    printf("Usage : %s [-bcdfht] [file_path] [partition_path]\n", progname);
    printf("   -b : print Boot sector in hex mode\n");
    printf("        command) debug.exfat -b /dev/block/mmcblk0p1\n");
    printf("   -c : print Cluster bit map info and map in hex mode\n");
    printf("        command) debug.exfat -c /dev/block/mmcblk0p1\n");
    printf("   -d : dump(extract) file. will work with -f option automatically.\n");
    printf("        command) debug.exfat -d [file_path] [partition_path]\n");
    printf("        eg)      debug.exfat -d /abc/12345 /dev/block/mmcblk0p1\n");
    printf("   -f : travel File path to get dentry info\n");
    printf("        command) debug.exfat -f [file_path] [partition_path]\n");
    printf("        eg)      If you want to extract file 12345 under directory abc, use below command\n");
    printf("                 debug.exfat -f /abc/12345 /dev/block/mmcblk0p1\n");
    printf("   -h : print Help usage\n");
    printf("        command) debug.exfat -h\n");
    printf("   -m : support mount mode\n");
    printf("        enter debugfs_exfat prompt and can listup file or change directory like shell.\n");
    printf("        it does not use mount system call.\n");
    printf("        command) debug.exfat -m /dev/block/mmcblk0p1\n");
    printf("        usable command in prompt) ls(or ll), cd, exit etc\n");
    printf("   -t : print FAT Table\n");
    printf("        command) debug.exfat -t /dev/block/mmcblk0p1\n");
    exit(-1);
}

int main(int argc, char *argv[])
{
    char buf[512] = {0,};
    int c=0;
    int print_boot_sector = 0;
    int print_cluster_bit_map = 0;
    int travel_file_dentry = 0;
    int print_fat_table = 0;
    int dump_file = 0;
    int mount_mode = 0;
    char f_path[MAX_PATH_LENGTH+1] = {0,};

    while ((c = getopt(argc, argv, "cbd:f:hmt")) != EOF)
        switch (c) {
            case 'b':
                print_boot_sector = 1;
                break;
            case 'c':
                print_cluster_bit_map = 1;
                break;
            case 'd':
                dump_file = 1;
                memcpy(f_path, optarg, strlen(optarg));
                break;
            case 'f':
                travel_file_dentry = 1;
                memcpy(f_path, optarg, strlen(optarg));
                break;
            case 'h':
                usage(argv[0]);
                break;
            case 'm':
                mount_mode = 1;
                break;
            case 't':
                print_fat_table = 1;
                break;
            default:
                usage(argv[0]);
        }

    memcpy(partition_path, argv[argc-1], strlen(argv[argc-1]));

    if (argc < 2) {
        printf("Please check your exfat partition path.(eg. mmcblk0p1)\n");
        usage(argv[0]);
    }

    start_prog();

    printf("\n======================= Filesystem Information ======================\n");
    fread(buf, sizeof(buf), 1, dev_fd); // READ BOOT SECTOR
    meta_print(buf);
    printf("======================================================================\n");

    if (print_boot_sector) {
        printf("\n============================ BOOT SECTOR ============================\n");
        hex_print(buf, sizeof(buf));
        printf("=====================================================================\n");
    }

    if (travel_file_dentry || dump_file) {
        printf("\n========================== Travel File Path ==========================\n");
        lookup_file_path(f_path, strlen(f_path), dump_file);
        printf("======================================================================\n");
    }

    if (print_cluster_bit_map) {
        printf("\n=========================== CLUSTER BIT MAP ==========================\n");
        get_cluster_bit_map_dentry();
        printf("======================================================================\n");
    }

    if (print_fat_table) {
        printf("\n============================== FAT TABLE =============================\n");
        get_fat_table();
        hex_print(fat_table, fat_length);
        printf("======================================================================\n");
    }

    if (mount_mode) {
        debugfs_main();
    }

    finish_prog();

    return 0;
}

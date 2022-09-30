#define FS_NAME_OFFSET 3
#define FS_NAME_LENGTH 8
#define VOLUME_LENGTH_OFFSET 72
#define VOLUME_LENGTH_LENGTH 8
#define FAT_OFFSET 80
#define FAT_LENGTH 4
#define FAT_LENGTH_OFFSET 84
#define FAT_LENGTH_LENGTH 4
#define CLUSTER_HEAP_OFFSET_OFFSET 88
#define CLUSTER_HEAP_OFFSET_LENGTH 4
#define CLUSTER_COUNT_OFFSET 92
#define CLUSTER_COUNT_LENGTH 4
#define FIRST_CLUSTER_OF_ROOT_DIR_OFFSET 96
#define FIRST_CLUSTER_OF_ROOT_DIR_LENGTH 4
#define VOLUME_SERIAL_NUM_OFFSET 100
#define VOLUME_SERIAL_NUM_LENGTH 4
#define VOLUME_FLAG_OFFSET 106
#define VOLUME_FLAG_LENGTH 2
#define BYTE_PER_SECTOR_SHIFT_OFFSET 108
#define BYTE_PER_SECTOR_SHIFT_LENGTH 1
#define SECTOR_PER_CLUSTER_SHIFT_OFFSET 109
#define SECTOR_PER_CLUSTER_SHIFT_LENGTH 1
#define NUM_OF_FAT_OFFSET 110
#define NUM_OF_FAT_LENGTH 1
#define PERCENT_OF_USE_OFFSET 112
#define PERCENT_OF_USE_LENGTH 1
#define BOOT_SIG_OFFSET 109
#define BOOT_SIG_LENGTH 1

#define MOUNTS_INFO "/proc/mounts"

#define MAX_NAME_LENGTH         255 /* max len of file name excluding NULL */
#define MAX_PATH_LENGTH         1530 /* we can get this value from statfs */

#define DENTRY_SIZE     32 /* directory entry size */
/* exFAT allows 8388608(256MB) directory entries */
#define MAX_EXFAT_DENTRIES  8388608

/* dentry types */
#define EXFAT_UNUSED        0x00    /* end of directory */
#define EXFAT_DELETE        (~0x80)
#define IS_EXFAT_DELETED(x) ((x) < 0x80) /* deleted file (0x01~0x7F) */
#define EXFAT_INVAL     0x80    /* invalid value */
#define EXFAT_BITMAP        0x81    /* allocation bitmap */
#define EXFAT_UPCASE        0x82    /* upcase table */
#define EXFAT_VOLUME        0x83    /* volume label */
#define EXFAT_FILE      0x85    /* file or dir */
#define EXFAT_GUID      0xA0
#define EXFAT_PADDING       0xA1
#define EXFAT_ACLTAB        0xA2
#define EXFAT_STREAM        0xC0    /* stream entry */
#define EXFAT_NAME      0xC1    /* file name entry */
#define EXFAT_ACL       0xC2    /* stream entry */

#define EXFAT_EOF_CLUSTER   0xFFFFFFFFu
#define EXFAT_BAD_CLUSTER   0xFFFFFFF7u
#define EXFAT_FREE_CLUSTER  0

/* file attributes */
#define ATTR_READONLY       0x0001
#define ATTR_HIDDEN     0x0002
#define ATTR_SYSTEM     0x0004
#define ATTR_VOLUME     0x0008
#define ATTR_SUBDIR     0x0010
#define ATTR_ARCHIVE        0x0020

struct dentry {
    char type;
    char generalsecondaryflags;
    char name_len;
    char f_name[MAX_NAME_LENGTH*2+1];
    unsigned short file_attribute;
    unsigned short name_hash;
    unsigned int first_cluster;
    unsigned long long data_length;
};

// Define below for using MS NameHash as it is.
typedef unsigned short UInt16;
typedef UInt16 WCHAR;
typedef unsigned char UCHAR;

void hex_print(char *buf, int length);
void exfat_statfs(void);
void meta_print(char *buf);
void lookup_file_path(char *f_path, int f_len, int dump_file);
void get_cluster_bit_map_dentry(void);
void get_fat_table(void);
void find_clusters(char generalsecondaryflags, unsigned int first_cluster, unsigned long long data_length, char *file_name, unsigned short file_attribute, int dump_file);
UInt16 NameHash(WCHAR * FileName, UCHAR   NameLength);
void debugfs_main(void);

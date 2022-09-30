#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "dump_exfat.h"

//extern char partition_path[256];
//extern char fs_name[9];
//extern long long vol_length;
//extern int fat_offset;
//extern int fat_length; // byte
extern int sector_size; // byte
extern int cluster_size; // byte
extern int cluster_heap_offset;
//extern int cluster_count;
extern int first_cluster_of_root_dir;
//extern char volume_flags[2];
//extern int number_of_fat;
//extern int percent_in_use;

extern FILE *dev_fd; // Use device fd as global, otherwise I need to open many time
extern char *fat_table; // FAT Table is needed for showing, file lookup and so on. Just read at once.

int show_del_file = 0;

struct current {
    char pwd[MAX_PATH_LENGTH+1];
    unsigned long long de_addr;
    struct current *next;
    struct current *prev;
} *current = NULL;

void add_node(char *dir, int len, unsigned long long de_addr)
{
    struct current *new;

    new = (struct current *)malloc(sizeof(struct current));
    if (!new) {
        printf("Memory Allocation Failed\n");
        exit(-1);
    }
    memset(new, 0, sizeof(struct current));

    while (current && current->next != NULL) {
        current = current->next;
    }

    if (current == NULL) {
        strncat(new->pwd, dir, len);
        new->de_addr = de_addr;
        new->next = NULL;
        new->prev = NULL;
        current = new;
        return ;
    } else {
        strcpy(new->pwd, current->pwd);
        if(strlen(current->pwd) != 1)
            strncat(new->pwd, "/", 1);
        strncat(new->pwd, dir, len);
        new->de_addr = de_addr;
        new->next = NULL;
        new->prev = current;
        current->next = new;
    }

    if (current->next != NULL) {
        current = current->next; // current will indicate last one always.
    }
}

int del_node(void)
{
    struct current *tmp = NULL;

    if (current->prev == NULL)
        return 1;   // current pwd is root already.
    tmp = current;
    current = current->prev;
    current->next = NULL;
    free(tmp);
    return 0;
}

void do_command_exit(void)
{
    printf(">> exit debug.exfat <<\n");
    exit(0);
}

void do_command_ls(void)
{
    char buf[512] = {0,};
    struct dentry de = {0,};
    int sum_of_dentries = 0;
    int f_name_offset = 0;
    int i = 0, j = 0, k =0;
    char asciiname[MAX_NAME_LENGTH+1] = {0,};
    int deleted = 0;
    unsigned long long next_addr = 0;
    unsigned int next_cluster = 0, current_cluster = ((current->de_addr - (cluster_heap_offset*sector_size))/cluster_size)+2;

    printf("d %12d\t.\n", cluster_size);
    if (strlen(current->pwd) != 1)
        printf("d %12d\t..\n", cluster_size);

    if (!fat_table)
        get_fat_table();
    next_addr = current->de_addr;

    do {
        fseek(dev_fd, next_addr+j, SEEK_SET);
        fread(buf, sizeof(buf), 1, dev_fd);
        do {
            sum_of_dentries++;
            memcpy(&de.type, buf+i, sizeof(char));
            if (de.type == EXFAT_FILE || de.type == (EXFAT_FILE&EXFAT_DELETE)) {
                if (de.type == (EXFAT_FILE&EXFAT_DELETE))
                    deleted = 1;
                memcpy(&de.file_attribute, buf+4+i, sizeof(unsigned short));
            } else if (de.type == EXFAT_STREAM || de.type == (EXFAT_STREAM&EXFAT_DELETE)) {
                memcpy(&de.name_len, buf+3+i, sizeof(char));
                memcpy(&de.data_length, buf+24+i, sizeof(unsigned long long));
            } else if (de.type == EXFAT_NAME || de.type == (EXFAT_NAME&EXFAT_DELETE)) {
                memcpy(de.f_name+f_name_offset, buf+2+i, 30);
                f_name_offset += 30;
                if (de.name_len <= f_name_offset/2) {
                    for (k=0; k<de.name_len*2; k++) {
                        if (k%2 != 1)
                            asciiname[k/2] = de.f_name[k];
                    }
                    if (deleted) {
                        deleted = 0;
                        if (show_del_file) {
                            printf("%c ", de.file_attribute & ATTR_SUBDIR ? 'd' : '-');
                            printf("%12lld\t", de.data_length);
                            printf("%s\t(deleted)\n", asciiname);
                        }
                    } else {
                        printf("%c ", de.file_attribute & ATTR_SUBDIR ? 'd' : '-');
                        printf("%12lld\t", de.data_length);
                        printf("%s\n", asciiname);
                    }
                    f_name_offset = 0;
                    memset(asciiname, 0, sizeof(asciiname));
                }
            } else if (de.type == EXFAT_UNUSED) {
                break;
            } else {
                i+=DENTRY_SIZE; // for 1 dentry
                continue;
            }
            i+=DENTRY_SIZE; // for 1 dentry
        } while(de.type && i<sizeof(buf));
        j += sizeof(buf);
        i = 0;

        if (j>=cluster_size) {
            memcpy(&next_cluster, fat_table+(current_cluster*4), 4);
            if (next_cluster == EXFAT_EOF_CLUSTER)
                break; // No more cluster chain
            next_addr=cluster_heap_offset*sector_size + (next_cluster-2)*cluster_size;
            current_cluster = next_cluster;
            j = 0;
        }

    } while(de.type && (sum_of_dentries < MAX_EXFAT_DENTRIES));
}

void do_command_cd(char *dir, int len)
{
    char *uniname = NULL;
    struct dentry de = {0,};
    unsigned short hash_tmp = 0;
    int found = 0;
    int i = 0, j = 0;
    char buf[512] = {0,};
    int sum_of_dentries = 0;
    int deleted = 0;

    if (!strncmp(dir, "/", 1) && len == 1) {
        while (!del_node()) {}
        return ;
    }

    if (!strncmp(dir, ".", 1) && len == 1) {
        return ;
    } else if (!strncmp(dir, "..", 2) && len == 2) {
        del_node();
        return ;
    }

    uniname = (char *)malloc(len*2);
    memset(uniname, 0, len*2);
    for(i=0; i<len*2; i++) {
        if(i%2)
            uniname[i] = 0;
        else
            uniname[i] = toupper(dir[i/2]);
    }

    // Pass ascii string length because NameHash makes it double.
    hash_tmp = (unsigned short)NameHash((WCHAR *)uniname, (UCHAR)len);

    do {
        fseek(dev_fd, current->de_addr+j, SEEK_SET);
        fread(buf, sizeof(buf), 1, dev_fd); // READ ROOT DENTRY

        i = 0;
        do {
            sum_of_dentries++;
            memcpy(&de.type, buf+i, sizeof(char));
            if (de.type == EXFAT_FILE || de.type == (EXFAT_FILE&EXFAT_DELETE)) {
                if (de.type == (EXFAT_FILE&EXFAT_DELETE))
                    deleted = 1;
                memcpy(&de.file_attribute, buf+4+i, sizeof(unsigned short));
            } else if (de.type == EXFAT_STREAM || de.type == (EXFAT_STREAM&EXFAT_DELETE)) {
                memcpy(&de.name_hash, buf+4+i, sizeof(unsigned short));
                memcpy(&de.first_cluster, buf+20+i, sizeof(unsigned int));
            } else {
                i+=DENTRY_SIZE; // for 1 dentry
                continue;
            }
            if (de.name_hash == hash_tmp) {
                found=1;
                // Change current dentry address
                if (de.file_attribute & ATTR_SUBDIR) {
                    if (show_del_file) {
                        add_node(dir, len, (cluster_heap_offset*sector_size)+(de.first_cluster-2)*cluster_size);
                        deleted = 0;
                    } else
                        add_node(dir, len, (cluster_heap_offset*sector_size)+(de.first_cluster-2)*cluster_size);
                } else
                    printf("%s is not directory\n", dir);
                break;
            }
            i+=DENTRY_SIZE; // for 1 dentry
        } while(i<sizeof(buf) && (de.type != EXFAT_UNUSED));

        j+=sizeof(buf); // for 1 sector
    } while(de.type != EXFAT_UNUSED && !found && (sum_of_dentries < MAX_EXFAT_DENTRIES));

    if (!found) {
        if (strlen(current->pwd) == 1)
            printf("cd %s%s : No such directory\n", current->pwd, dir);
        else
            printf("cd %s/%s : No such directory\n", current->pwd, dir);
    }
}

void do_command_pwd(void)
{
    printf("current working directory : %s\n", current->pwd);
}

void do_command_get_env(char *env_name, int len)
{
    if (!strncmp(env_name, "del", 3) && len == 3)
        printf("%s\n", show_del_file?"enabled":"disabled");
}

void do_command_set_env(char *env_name, int env_len, char *val, int val_len)
{
    if (!strncmp(env_name, "del", 3) && env_len == 3)
        if((!strncmp(val, "1", 1) || !strncmp(val, "0", 1)) && val_len == 1)
            show_del_file = atoi(val);
        else
            printf("%s is invalid. \"set del\" shold have 1 or 0.\n", val);
    else
        printf("%s is invalid parameter.\n", env_name);
}

void do_command_help(void)
{
    printf("Usable command list\n");
    printf("    cd [dir]            : change directory\n");
    printf("    ls|ll               : listup file\n");
    printf("    get [var]           : get environment value\n");
    printf("    set [var] [val]     : set environment value\n");
    printf("    - get/set del [0|1] : show deleted file together\n");
    printf("    pwd                 : print present working directory\n");
    printf("    exit|q|quit         : exit program\n");
    printf("    help                : print usable command list\n\n");
}

void execute_line(char *buf)
{
    char *str = NULL;
    int i = 0;
    char command[32] = {0,};
    char param_1[MAX_NAME_LENGTH+1] = {0,};
    char param_2[MAX_NAME_LENGTH+1] = {0,};

    str = strtok(buf, " ");
    while (str != NULL) {
        if (!i) {
            memcpy(command, str, strlen(str));
        } else if(i == 1) {
            memcpy(param_1, str, strlen(str));
        } else if(i == 2) {
            memcpy(param_2, str, strlen(str));
        }
        i++;
        str = strtok(NULL, " ");
    }

    if (!strncmp(command, "exit", 4) || !strncmp(command, "q", 1) ||!strncmp(command, "quit", 4))
        do_command_exit();
    else if (!strncmp(command, "ls", 2) || !strncmp(command, "ll", 2)) {
        do_command_ls();
    } else if (!strncmp(command, "cd", 2)) {
        if (!strncmp(param_1, "/", 1) && strlen(param_1) == 1)
            do_command_cd(param_1, strlen(param_1));
        str = strtok(param_1, "/");
        while (str != NULL) {
            do_command_cd(str, strlen(str));
            str = strtok(NULL, "/");
        }
    } else if (!strncmp(command, "pwd", 3))
        do_command_pwd();
    else if (!strncmp(command, "get", 3))
        do_command_get_env(param_1, strlen(param_1));
    else if (!strncmp(command, "set", 3))
        do_command_set_env(param_1, strlen(param_1), param_2, strlen(param_2));
    else if (!strncmp(command, "help", 4))
        do_command_help();
    else
        printf("%s : unknown command\n", command);
}

void debugfs_main(void)
{
    FILE *fd = stdin;
    char buf[MAX_NAME_LENGTH+1] = {0,};
    char *cp = NULL;

    fflush(stdout);
    fflush(stderr);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    add_node("/", 1, (cluster_heap_offset*sector_size)+(first_cluster_of_root_dir-2)*cluster_size);

    while (!feof(fd)) {
        printf("debugfs_exfat: %s # ", current->pwd);
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;
        if ((cp = strchr(buf, '\n')))
            *cp = 0;
        if ((cp = strchr(buf, '\r')))
            *cp = 0;
        if (!strlen(buf))
            continue;

        execute_line(buf);
    }
}

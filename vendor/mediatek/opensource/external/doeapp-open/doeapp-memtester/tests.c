/*
 * Very simple but very effective user-space memory tester.
 * Originally by Simon Kirby <sim@stormix.com> <sim@neato.org>
 * Version 2 by Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Version 3 not publicly released.
 * Version 4 rewrite:
 * Copyright (C) 2004-2012 Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Licensed under the terms of the GNU General Public License version 2 (only).
 * See the file COPYING for details.
 *
 * This file contains the functions for the actual tests, called from the
 * main routine in memtester.c.  See other comments in that file.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "types.h"
#include "sizes.h"
#include "memtester.h"
#include "util.h"

char progress[] = "-\\|/";
#define PROGRESSLEN 4
#define PROGRESSOFTEN 2500
#define ONE 0x00000001L

/* Function definitions. */

int report_err(const char *module, const char *message)
{
    pid_t pid;
    const char *argv[] = { "dreport", "-c", NULL, "-m", NULL, NULL };

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fail to fork (errno:%d)\n", errno);
        return -1;
    } else if (pid == 0) {
        argv[2] = module;
        argv[4] = message;

        execvp(argv[0], (char *const *)argv);

        fprintf(stderr, "Cannot execvp (errno:%d)\n", errno);
        return -1;
    } else {
        return 0;
    }
}

char *addr2dram(ull addr)
{
#define INPUT_MAX 32
    static char input[INPUT_MAX], *output = NULL;
    static int page_sz;
    int fd, n, ret;
    static const char *DRAM2ADDR_PATH =
                      "/sys/bus/platform/drivers/emicen_drv/emicen_addr2dram";

    if (!page_sz)
        page_sz = getpagesize();

    if (!output) {
        output = (char *)malloc(page_sz);
        if (!output) {
            fprintf(stderr, "DEBUG: Failed to malloc(%d)\n", errno);
            return NULL;
        }
    }

    n = snprintf(input, INPUT_MAX, "%llx", addr);
    if (n < 0) {
        fprintf(stderr, "DEBUG: Failed to snprintf(%d)\n", errno);
        return NULL;
    }

    /*
     * echo address > DRAM2DAAR_PATH
     */

    fd = open(DRAM2ADDR_PATH, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "DEBUG: Failed to open %s(%d)\n",
                DRAM2ADDR_PATH, errno);
        return NULL;
    }

    n = 0;
    while (n < strlen(input)) {
        ret = write(fd, input, strlen(input));
        if (ret == -1) {
            fprintf(stderr, "DEBUG: Failed to write %s(%d)\n",
                    DRAM2ADDR_PATH, errno);
            output[0] = '\0';
            goto addr2dram_exit;
        } else
            n += ret;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "DEBUG: Failed to close %s(%d)\n",
                DRAM2ADDR_PATH, errno);
        return NULL;
    }

    /*
     * cat DRAM2DAAR_PATH
     */

    fd = open(DRAM2ADDR_PATH, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "DEBUG: Failed to open %s(%d)\n",
                DRAM2ADDR_PATH, errno);
        return NULL;
    }

    n = 0;
    while (n < page_sz) {
        ret = read(fd, output + n, page_sz);
        if (ret == 0)
            break;
        else if (n == -1) {
            fprintf(stderr, "DEBUG: Failed to read %s(%d)\n",
                    DRAM2ADDR_PATH, errno);
            output[0] = '\0';
        } else
            n += ret;
    }

addr2dram_exit:
    if (close(fd) == -1) {
        fprintf(stderr, "DEBUG: Failed to close %s(%d)\n",
                DRAM2ADDR_PATH, errno);
    }

    return output;
}

int compare_regions(ulv *bufa, ulv *bufb, size_t count) {
    int r = 0;
    size_t i;
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    ul v1, v2;
    off_t physaddr;

    for (i = 0; i < count; i++, p1++, p2++) {
        v1 = *p1;
        v2 = *p2;
        if (v1 != v2) {
            if (use_phys) {
                physaddr = physaddrbase + (i * sizeof(ul));
                fprintf(stderr, 
                        "FAILURE: 0x%08lx != 0x%08lx at physical address "
                        "0x%08lx.\n", 
                        v1, v2, physaddr);
                fprintf(stderr,
                        "DEBUG: phyaddr to DRAM point of view\n%s\n",
                        addr2dram(physaddr));
            } else {
                fprintf(stderr, 
                        "FAILURE: 0x%08lx != 0x%08lx at offset 0x%08lx.\n", 
                        v1, v2, (ul) (i * sizeof(ul)));
                fprintf(stderr,
                        "DEBUG: vaddr1 %p paddr1 0x%llx "
                        "vaddr2 %p paddr2 0x%llx.\n",
                        p1, virt2phys((ull)p1), \
                        p2, virt2phys((ull)p2));
                fprintf(stderr,
                        "DEBUG: paddr1 to DRAM point of view\n%s\n",
                        addr2dram(virt2phys((ull)p1)));
                fprintf(stderr,
                        "DEBUG: paddr2 to DRAM point of view\n%s\n",
                        addr2dram(virt2phys((ull)p2)));
                if (find_byteinvert(v1, v2)) {
                    fprintf(stderr,
                            "DEBUG: find byte-invert.\n");
                }
                if (find_bitflips(v1, v2)) {
                    fprintf(stderr,
                            "DEBUG: find bitflips.\n");
                }
            }
            /* printf("Skipping to next test..."); */
            r = -1;
            report_err("DoEapp:memtester", "Compare failed");
        }
    }
    return r;
}

int test_stuck_address(ulv *bufa, size_t count) {
    ulv *p1 = bufa;
    unsigned int j;
    size_t i;
    off_t physaddr;

    printf("           ");
    fflush(stdout);
    for (j = 0; j < 16; j++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        p1 = (ulv *) bufa;
        printf("setting %3u", j);
        fflush(stdout);
        for (i = 0; i < count; i++) {
            *p1 = ((j + i) % 2) == 0 ? (ul) p1 : ~((ul) p1);
            *p1++;
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
        p1 = (ulv *) bufa;
        for (i = 0; i < count; i++, p1++) {
            if (*p1 != (((j + i) % 2) == 0 ? (ul) p1 : ~((ul) p1))) {
                if (use_phys) {
                    physaddr = physaddrbase + (i * sizeof(ul));
                    fprintf(stderr, 
                            "FAILURE: possible bad address line at physical "
                            "address 0x%08lx.\n", 
                            physaddr);
                    fprintf(stderr,
                            "DEBUG: physical address to "
                            "DRAM point of view\n%s\n",
                            addr2dram((ull)physaddr));
                } else {
                    fprintf(stderr, 
                            "FAILURE: possible bad address line at offset "
                            "0x%08lx.\n", 
                            (ul) (i * sizeof(ul)));
                }
                printf("Skipping to next test...\n");
                fflush(stdout);
                report_err("DoEapp:memtester", "Test stuck failed");
                return -1;
            }
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

int test_random_value(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    ul j = 0;
    size_t i;

    putchar(' ');
    fflush(stdout);
    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = rand_ul();
        if (!(i % PROGRESSOFTEN)) {
            putchar('\b');
            putchar(progress[++j % PROGRESSLEN]);
            fflush(stdout);
        }
    }
    printf("\b \b");
    fflush(stdout);
    return compare_regions(bufa, bufb, count);
}

int test_xor_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ ^= q;
        *p2++ ^= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_sub_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ -= q;
        *p2++ -= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_mul_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ *= q;
        *p2++ *= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_div_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        if (!q) {
            q++;
        }
        *p1++ /= q;
        *p2++ /= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_or_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ |= q;
        *p2++ |= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_and_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ &= q;
        *p2++ &= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_seqinc_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = (i + q);
    }
    return compare_regions(bufa, bufb, count);
}

int test_solidbits_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    printf("           ");
    fflush(stdout);
    for (j = 0; j < 64; j++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        q = (j % 2) == 0 ? UL_ONEBITS : 0;
        printf("setting %3u", j);
        fflush(stdout);
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

int test_checkerboard_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    printf("           ");
    fflush(stdout);
    for (j = 0; j < 64; j++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        q = (j % 2) == 0 ? CHECKERBOARD1 : CHECKERBOARD2;
        printf("setting %3u", j);
        fflush(stdout);
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

int test_blockseq_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    printf("           ");
    fflush(stdout);
    for (j = 0; j < 256; j++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        printf("setting %3u", j);
        fflush(stdout);
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (ul) UL_BYTE(j);
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

int test_walkbits0_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    printf("           ");
    fflush(stdout);
    for (j = 0; j < UL_LEN * 2; j++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        printf("setting %3u", j);
        fflush(stdout);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = ONE << j;
            } else { /* Walk it back down. */
                *p1++ = *p2++ = ONE << (UL_LEN * 2 - j - 1);
            }
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

int test_walkbits1_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    printf("           ");
    fflush(stdout);
    for (j = 0; j < UL_LEN * 2; j++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        printf("setting %3u", j);
        fflush(stdout);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << j);
            } else { /* Walk it back down. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << (UL_LEN * 2 - j - 1));
            }
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

int test_bitspread_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    printf("           ");
    fflush(stdout);
    for (j = 0; j < UL_LEN * 2; j++) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        printf("setting %3u", j);
        fflush(stdout);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = (i % 2 == 0)
                    ? (ONE << j) | (ONE << (j + 2))
                    : UL_ONEBITS ^ ((ONE << j)
                                    | (ONE << (j + 2)));
            } else { /* Walk it back down. */
                *p1++ = *p2++ = (i % 2 == 0)
                    ? (ONE << (UL_LEN * 2 - 1 - j)) | (ONE << (UL_LEN * 2 + 1 - j))
                    : UL_ONEBITS ^ (ONE << (UL_LEN * 2 - 1 - j)
                                    | (ONE << (UL_LEN * 2 + 1 - j)));
            }
        }
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

int test_bitflip_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j, k;
    ul q;
    size_t i;

    printf("           ");
    fflush(stdout);
    for (k = 0; k < UL_LEN; k++) {
        q = ONE << k;
        for (j = 0; j < 8; j++) {
            printf("\b\b\b\b\b\b\b\b\b\b\b");
            q = ~q;
            printf("setting %3u", k * 8 + j);
            fflush(stdout);
            p1 = (ulv *) bufa;
            p2 = (ulv *) bufb;
            for (i = 0; i < count; i++) {
                *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
            }
            printf("\b\b\b\b\b\b\b\b\b\b\b");
            printf("testing %3u", k * 8 + j);
            fflush(stdout);
            if (compare_regions(bufa, bufb, count)) {
                return -1;
            }
        }
    }
    printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
    fflush(stdout);
    return 0;
}

#ifdef TEST_NARROW_WRITES    
int test_8bit_wide_random(ulv* bufa, ulv* bufb, size_t count) {
    u8v *p1, *t;
    ulv *p2;
    int attempt;
    unsigned int b, j = 0;
    size_t i;

    putchar(' ');
    fflush(stdout);
    for (attempt = 0; attempt < 2;  attempt++) {
        if (attempt & 1) {
            p1 = (u8v *) bufa;
            p2 = bufb;
        } else {
            p1 = (u8v *) bufb;
            p2 = bufa;
        }
        for (i = 0; i < count; i++) {
            t = mword8.bytes;
            *p2++ = mword8.val = rand_ul();
            for (b=0; b < UL_LEN/8; b++) {
                *p1++ = *t++;
            }
            if (!(i % PROGRESSOFTEN)) {
                putchar('\b');
                putchar(progress[++j % PROGRESSLEN]);
                fflush(stdout);
            }
        }
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b \b");
    fflush(stdout);
    return 0;
}

int test_16bit_wide_random(ulv* bufa, ulv* bufb, size_t count) {
    u16v *p1, *t;
    ulv *p2;
    int attempt;
    unsigned int b, j = 0;
    size_t i;

    putchar( ' ' );
    fflush( stdout );
    for (attempt = 0; attempt < 2; attempt++) {
        if (attempt & 1) {
            p1 = (u16v *) bufa;
            p2 = bufb;
        } else {
            p1 = (u16v *) bufb;
            p2 = bufa;
        }
        for (i = 0; i < count; i++) {
            t = mword16.u16s;
            *p2++ = mword16.val = rand_ul();
            for (b = 0; b < UL_LEN/16; b++) {
                *p1++ = *t++;
            }
            if (!(i % PROGRESSOFTEN)) {
                putchar('\b');
                putchar(progress[++j % PROGRESSLEN]);
                fflush(stdout);
            }
        }
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    printf("\b \b");
    fflush(stdout);
    return 0;
}
#endif

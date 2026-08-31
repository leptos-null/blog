// symlist -- list the symbol table of a Mach-O file.
//
//   symlist <path-to-mach-o>
//
// Reads LC_SYMTAB and prints one line per symbol:
//
//   <name>: <location> [<flags>]
//
// where <location> depends on the symbol's type:
//
//   defined (N_SECT, N_ABS)  the address the symbol resolves to
//   undefined (N_UNDF,       where the symbol is expected to come from: the
//    N_PBUD)                 dylib named by its two-level namespace ordinal,
//                            or one of the special ordinals -- (this image),
//                            (executable), (dynamic lookup). Object files
//                            print (undefined); an image built for the flat
//                            namespace prints (flat namespace)
//   common (N_UNDF, legacy)  the number of bytes the linker should allocate;
//                            such a symbol has a size instead of an address
//   indirect (N_INDR)        the name this symbol aliases
//
// Accepts 64-bit Mach-O files, either thin or as slices within a FAT file.
// Each slice is printed under a header naming its architecture, or its raw
// cputype/cpusubtype when this program doesn't recognize it. A slice that
// isn't 64-bit Mach-O is reported on stderr and makes the exit status
// non-zero; the remaining slices are still listed.
// Debug (N_STAB) entries and linker-synthesized symbols such as
// __mh_execute_header are omitted.
//
// Roughly a stripped-down `nm -m`, written to show where the information
// comes from. Input is assumed to be well-formed: offsets within a slice
// are largely trusted, so this is not suitable for untrusted files.
//
// Build: clang -Wall -Wextra -o symlist symlist.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mach/machine.h>
#include <mach-o/loader.h>
#include <mach-o/ldsyms.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
#include <libkern/OSByteOrder.h>

typedef struct {
    char **paths;
    size_t count;
} dylib_list_t;

static void dylib_list_append(dylib_list_t *list, const char *path) {
    char **paths = realloc(list->paths, sizeof(char *) * (list->count + 1));
    if (paths == NULL) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    list->paths = paths;

    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    list->paths[list->count] = path_copy;
    list->count++;
}

static void dylib_list_free(dylib_list_t *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
}

static const char *arch_name(cpu_type_t cputype, cpu_subtype_t cpusubtype) {
    uint32_t const subtype = (uint32_t)cpusubtype & ~CPU_SUBTYPE_MASK;
    switch (cputype) {
        case CPU_TYPE_ARM64:
            return (subtype == CPU_SUBTYPE_ARM64E) ? "arm64e" : "arm64";
        case CPU_TYPE_X86_64:
            return "x86_64";
        default:
            return NULL;
    }
}

// Symbols the linker synthesizes itself
static bool is_internal_symbol(const char *name) {
    static const char *internal[] = {
        _MH_EXECUTE_SYM,
        _MH_DYLIB_SYM,
        _MH_BUNDLE_SYM,
    };
    for (size_t i = 0; i < sizeof(internal) / sizeof(internal[0]); i++) {
        if (strcmp(name, internal[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void append_flag(char *buf, size_t bufsize, const char *flag) {
    if (buf[0] != '\0') {
        strlcat(buf, ", ", bufsize);
    }
    strlcat(buf, flag, bufsize);
}

static const char *library_path_for_ordinal(const dylib_list_t *dylibs, uint8_t ordinal) {
    switch (ordinal) {
        case SELF_LIBRARY_ORDINAL:
            return "(this image)";
        case DYNAMIC_LOOKUP_ORDINAL:
            return "(dynamic lookup)";
        case EXECUTABLE_ORDINAL:
            return "(executable)";
    }
    // The special ordinals handled above are 0x00, 0xfe, and 0xff, so anything
    // reaching here is a 1-based index in [1, MAX_LIBRARY_ORDINAL].
    if (ordinal <= dylibs->count) {
        return dylibs->paths[ordinal - 1];
    }
    return "(unknown library)";
}

// `slice` points at a mach_header_64, either the whole file (thin) or one
// architecture's region within a FAT file (all offsets inside a Mach-O image
// are relative to that image's own base, so this works either way).
static bool parse_slice(const uint8_t *slice, uint64_t slice_size) {
    if (slice_size < sizeof(struct mach_header_64)) {
        fprintf(stderr, "slice too small to contain a mach_header_64\n");
        return false;
    }

    const struct mach_header_64 *header = (const struct mach_header_64 *)slice;
    if (header->magic != MH_MAGIC_64) {
        fprintf(stderr, "unsupported Mach-O slice\n");
        return false;
    }

    const bool is_two_level = (header->flags & MH_TWOLEVEL) != 0;
    // MH_TWOLEVEL is never set on object files, but an object file isn't
    // flat-namespace either: its undefined symbols have no library yet.
    const bool is_object_file = (header->filetype == MH_OBJECT);

    dylib_list_t dylibs = {0};

    const struct nlist_64 *symtab = NULL;
    uint32_t nsyms = 0;
    const uint8_t *strtab = NULL;
    uint32_t strtab_size = 0;

    const uint8_t *cmd_ptr = slice + sizeof(struct mach_header_64);
    for (uint32_t i = 0; i < header->ncmds; i++) {
        const struct load_command *cmd = (const struct load_command *)cmd_ptr;

        switch (cmd->cmd) {
            case LC_SYMTAB: {
                const struct symtab_command *symtab_cmd = (const struct symtab_command *)cmd;
                symtab = (const struct nlist_64 *)(slice + symtab_cmd->symoff);
                nsyms = symtab_cmd->nsyms;
                strtab = slice + symtab_cmd->stroff;
                strtab_size = symtab_cmd->strsize;
                break;
            }
            case LC_LOAD_DYLIB:
            case LC_LOAD_WEAK_DYLIB:
            case LC_REEXPORT_DYLIB:
            case LC_LOAD_UPWARD_DYLIB:
            case LC_LAZY_LOAD_DYLIB: {
                const struct dylib_command *dylib_cmd = (const struct dylib_command *)cmd;
                const char *path = (const char *)cmd_ptr + dylib_cmd->dylib.name.offset;
                dylib_list_append(&dylibs, path);
                break;
            }
            default:
                break;
        }

        cmd_ptr += cmd->cmdsize;
    }

    if (symtab == NULL) {
        fprintf(stderr, "no LC_SYMTAB found\n");
        dylib_list_free(&dylibs);
        return false;
    }

    for (uint32_t i = 0; i < nsyms; i++) {
        const struct nlist_64 *entry = &symtab[i];
        if (entry->n_type & N_STAB) {
            continue;
        }
        if (entry->n_un.n_strx == 0 || entry->n_un.n_strx >= strtab_size) {
            continue;
        }
        const char *name = (const char *)strtab + entry->n_un.n_strx;
        if (is_internal_symbol(name)) {
            continue;
        }

        uint8_t const type = entry->n_type & N_TYPE;
        // N_UNDF with a non-zero n_value is a common symbol: a definition, not a reference.
        const bool is_undefined = (type == N_UNDF && entry->n_value == 0) || (type == N_PBUD);
        const bool is_indirect = (type == N_INDR);

        char flags[128] = {0};
        append_flag(flags, sizeof(flags), (entry->n_type & N_EXT) ? "extern" : "local");
        if (entry->n_type & N_PEXT) {
            append_flag(flags, sizeof(flags), "private-extern");
        }
        // N_WEAK_DEF and N_REF_TO_WEAK are the same bit. Which one it means
        // depends on whether this symbol is defined here or referenced from elsewhere.
        if (entry->n_desc & N_WEAK_DEF) {
            append_flag(flags, sizeof(flags), is_undefined ? "ref-to-weak" : "weak-def");
        }
        if (entry->n_desc & N_WEAK_REF) {
            append_flag(flags, sizeof(flags), "weak-ref");
        }

        // `location` points at `location_buf`, or at a string owned by the file
        // (a dylib path, an aliased name) or by this program. Symbol names have
        // no length bound, so nothing is copied into a fixed-size buffer here.
        char location_buf[32];
        const char *location;

        switch (type) {
            case N_SECT:
                snprintf(location_buf, sizeof(location_buf), "0x%016llx", entry->n_value);
                location = location_buf;
                break;
            case N_ABS:
                append_flag(flags, sizeof(flags), "absolute");
                snprintf(location_buf, sizeof(location_buf), "0x%016llx", entry->n_value);
                location = location_buf;
                break;
            case N_INDR: {
                // For an indirect symbol, n_value is a string table index for
                // the aliased name, not an address.
                //
                // Hard to produce with the current toolchain: `-Wl,-alias`
                // emits an ordinary N_SECT definition, and `.indirect_symbol`
                // is rejected outside a stub section. Exercised by patching an
                // nlist_64 entry to N_INDR by hand.
                append_flag(flags, sizeof(flags), "indirect");
                uint64_t alias_strx = entry->n_value;
                location = (alias_strx < strtab_size) ? (const char *)strtab + alias_strx : "(unknown)";
                break;
            }
            case N_UNDF:
            case N_PBUD:
                if (type == N_UNDF && entry->n_value != 0) {
                    // Legacy "common" symbol: tentative definition with a size, not an address.
                    append_flag(flags, sizeof(flags), "common");
                    snprintf(location_buf, sizeof(location_buf), "size 0x%llx", entry->n_value);
                    location = location_buf;
                } else if (is_object_file) {
                    location = "(undefined)";
                } else if (is_two_level) {
                    uint8_t ordinal = GET_LIBRARY_ORDINAL(entry->n_desc);
                    location = library_path_for_ordinal(&dylibs, ordinal);
                } else {
                    location = "(flat namespace)";
                }
                break;
            default:
                location = "(unknown)";
                break;
        }

        printf("%s: %s%s [%s]\n", name, is_indirect ? "-> " : "", location, flags);
    }

    dylib_list_free(&dylibs);
    return true;
}

static bool parse_fat_32(const uint8_t *base, size_t size) {
    if (size < sizeof(struct fat_header)) {
        fprintf(stderr, "file too small to contain a fat_header\n");
        return false;
    }

    const struct fat_header *fat = (const struct fat_header *)base;
    uint32_t nfat_arch = OSSwapBigToHostInt32(fat->nfat_arch);
    if (nfat_arch > (size - sizeof(struct fat_header)) / sizeof(struct fat_arch)) {
        fprintf(stderr, "fat header describes more architectures than the file can hold\n");
        return false;
    }
    const struct fat_arch *arches = (const struct fat_arch *)(base + sizeof(struct fat_header));

    bool ok = true;
    for (uint32_t i = 0; i < nfat_arch; i++) {
        cpu_type_t cputype = (cpu_type_t)OSSwapBigToHostInt32((uint32_t)arches[i].cputype);
        cpu_subtype_t cpusubtype = (cpu_subtype_t)OSSwapBigToHostInt32((uint32_t)arches[i].cpusubtype);
        uint32_t offset = OSSwapBigToHostInt32(arches[i].offset);
        uint32_t slice_size = OSSwapBigToHostInt32(arches[i].size);

        const char *name = arch_name(cputype, cpusubtype);
        if (name != NULL) {
            printf("-- %s --\n", name);
        } else {
            printf("-- cputype 0x%x, cpusubtype 0x%x --\n", cputype, cpusubtype);
        }

        if (offset <= size && slice_size <= size - offset) {
            if (!parse_slice(base + offset, slice_size)) {
                ok = false;
            }
        } else {
            fprintf(stderr, "slice out of bounds\n");
            ok = false;
        }
    }
    return ok;
}

static bool parse_fat_64(const uint8_t *base, size_t size) {
    if (size < sizeof(struct fat_header)) {
        fprintf(stderr, "file too small to contain a fat_header\n");
        return false;
    }

    const struct fat_header *fat = (const struct fat_header *)base;
    uint32_t nfat_arch = OSSwapBigToHostInt32(fat->nfat_arch);
    if (nfat_arch > (size - sizeof(struct fat_header)) / sizeof(struct fat_arch_64)) {
        fprintf(stderr, "fat header describes more architectures than the file can hold\n");
        return false;
    }
    const struct fat_arch_64 *arches = (const struct fat_arch_64 *)(base + sizeof(struct fat_header));

    bool ok = true;
    for (uint32_t i = 0; i < nfat_arch; i++) {
        cpu_type_t cputype = (cpu_type_t)OSSwapBigToHostInt32((uint32_t)arches[i].cputype);
        cpu_subtype_t cpusubtype = (cpu_subtype_t)OSSwapBigToHostInt32((uint32_t)arches[i].cpusubtype);
        uint64_t offset = OSSwapBigToHostInt64(arches[i].offset);
        uint64_t slice_size = OSSwapBigToHostInt64(arches[i].size);

        const char *name = arch_name(cputype, cpusubtype);
        if (name != NULL) {
            printf("-- %s --\n", name);
        } else {
            printf("-- cputype 0x%x, cpusubtype 0x%x --\n", cputype, cpusubtype);
        }

        if (offset <= size && slice_size <= size - offset) {
            if (!parse_slice(base + offset, slice_size)) {
                ok = false;
            }
        } else {
            fprintf(stderr, "slice out of bounds\n");
            ok = false;
        }
    }
    return ok;
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <path-to-mach-o>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat");
        close(fd);
        return EXIT_FAILURE;
    }

    size_t size = (size_t)st.st_size;
    void *mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    const uint8_t *base = (const uint8_t *)mapped;

    if (size < sizeof(uint32_t)) {
        fprintf(stderr, "file too small to be a Mach-O file\n");
        munmap(mapped, size);
        return EXIT_FAILURE;
    }

    uint32_t magic;
    memcpy(&magic, base, sizeof(magic));

    int status = EXIT_SUCCESS;
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        if (!parse_fat_32(base, size)) {
            status = EXIT_FAILURE;
        }
    } else if (magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
        if (!parse_fat_64(base, size)) {
            status = EXIT_FAILURE;
        }
    } else if (magic == MH_MAGIC_64) {
        if (!parse_slice(base, size)) {
            status = EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "unsupported file\n");
        status = EXIT_FAILURE;
    }

    munmap(mapped, size);
    return status;
}

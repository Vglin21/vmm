#ifndef VGLINS_MEMORY_MAP_H
#define VGLINS_MEMORY_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#ifndef UINT_ALIASES
#define UINT_ALIASES

typedef uint8_t  byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

#endif

#ifndef VMM_DECL
    #ifdef VMM_STATIC
        #define VMM_DECL static
    #else
        #define VMM_DECL extern
    #endif
#endif

#ifndef VMM_MAX_CHUNK_COUNT
    #define VMM_MAX_CHUNK_COUNT 256
#endif

#ifndef VMM_EMPTY_CHUNK
    #define VMM_EMPTY_CHUNK ((dword)(-1))
#endif

typedef byte *vmm_chunk_ptr;

typedef enum {
    VMM_CHUNK_SIZE_1_BYTE    = 0,
    VMM_CHUNK_SIZE_2_BYTES   = 1,
    VMM_CHUNK_SIZE_4_BYTES   = 2,
    VMM_CHUNK_SIZE_8_BYTES   = 3,
    VMM_CHUNK_SIZE_16_BYTES  = 4,
    VMM_CHUNK_SIZE_32_BYTES  = 5,
    VMM_CHUNK_SIZE_64_BYTES  = 6,
    VMM_CHUNK_SIZE_128_BYTES = 7,
    VMM_CHUNK_SIZE_256_BYTES = 8,
    VMM_CHUNK_SIZE_512_BYTES = 9,
    VMM_CHUNK_SIZE_1_KB      = 10,
    VMM_CHUNK_SIZE_2_KB      = 11,
    VMM_CHUNK_SIZE_4_KB      = 12,
    VMM_CHUNK_SIZE_8_KB      = 13,
    VMM_CHUNK_SIZE_16_KB     = 14,
    VMM_CHUNK_SIZE_32_KB     = 15,
    VMM_CHUNK_SIZE_64_KB     = 16
} vmm_chunk_size;

#ifdef __cplusplus
extern "C" {
#endif

VMM_DECL void vmm_set_chunk_size(vmm_chunk_size chunk_size);

VMM_DECL bool vmm_allocate_chunks(size_t count);
VMM_DECL void vmm_free_chunks(void);

VMM_DECL void vmm_assign_addr_to_chunk(dword addr, word chunk_id, bool read_only);
VMM_DECL void vmm_assign_addr_to_chunks(dword addr, word chunk_id, bool read_only, size_t count);

VMM_DECL bool vmm_insert_rom(const char *rom_path);
VMM_DECL bool vmm_insert_rom_w(const wchar_t *rom_path);
VMM_DECL void vmm_remove_rom(void);

VMM_DECL bool vmm_read_rom(dword addr, word chunk_id, size_t count);
VMM_DECL bool vmm_read_rom_from_array(dword addr, word chunk_id, size_t count, byte *rom, size_t rom_size);

VMM_DECL inline void vmm_write(dword addr, byte value);
VMM_DECL inline byte vmm_read(dword addr);

VMM_DECL vmm_chunk_ptr vmm_get_chunk_ptr(word chunk_id);

#ifdef _cplusplus
}
#endif

#define VMM_IMPLEMENTATION

#if defined(VMM_IMPLEMENTATION) || defined(VGLIN_IMPLEMENTATION)
#define VMM_IMPLEMENTATION_INCLUDED (1)

#ifndef VMM_DEF
    #ifdef VMM_STATIC
        #define VMM_DEF static
    #else
        #define VMM_DEF
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    dword addr;
    bool read_only;
} vmm_chunk;

typedef struct {
    byte addr_shift;
    dword chunk_size;
    dword mask;
    byte *memory;
    size_t memory_size;
    vmm_chunk chunks[VMM_MAX_CHUNK_COUNT];
    FILE *rom;
    size_t rom_size;
} vmm_state;

VMM_DEF vmm_state _vmm = {0};

VMM_DEF void vmm_set_chunk_size(vmm_chunk_size chunk_size) {
    if (chunk_size <= VMM_CHUNK_SIZE_64_KB) {
        _vmm.addr_shift = chunk_size;
        _vmm.chunk_size = 1 << chunk_size;
        _vmm.mask = _vmm.chunk_size - 1;
    }
}

VMM_DEF bool vmm_allocate_chunks(size_t count) {
    byte *new_memory;
    if (!(new_memory = malloc(_vmm.memory_size = _vmm.chunk_size * count))) return false;

    memset(_vmm.chunks, 0xff, sizeof(_vmm.chunks));

    if (_vmm.memory) free(_vmm.memory);
    _vmm.memory = new_memory;
    memset(_vmm.memory, 0, _vmm.memory_size);

    return true;
}
VMM_DEF void vmm_free_chunks(void) {
    if (_vmm.memory) {
        free(_vmm.memory);
        _vmm.memory = NULL;
    }
}

VMM_DEF void vmm_assign_addr_to_chunk(dword addr, word chunk_id, bool read_only) {
    if (chunk_id >= VMM_MAX_CHUNK_COUNT) return;

    if (addr < _vmm.memory_size && addr + _vmm.chunk_size <= _vmm.memory_size) {
        _vmm.chunks[chunk_id].addr = addr;
        _vmm.chunks[chunk_id].read_only = read_only;
    }
}
VMM_DEF void vmm_assign_addr_to_chunks(dword addr, word chunk_id, bool read_only, size_t count) {
    for (size_t i = 0; i < count && chunk_id + i < VMM_MAX_CHUNK_COUNT; ++i)
        vmm_assign_addr_to_chunk(addr + i * _vmm.chunk_size, chunk_id + i, read_only);
}

VMM_DEF bool _vmm_insert_rom(FILE *file) {
    if (!file) return false;

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (!fsize) {
        fclose(file);
        return false;
    }

    if (_vmm.rom) fclose(_vmm.rom);
    _vmm.rom = file;
    _vmm.rom_size = fsize;

    return true;
}

VMM_DEF bool vmm_insert_rom(const char *rom_path) {
    if (!rom_path) return false;
    return _vmm_insert_rom(fopen(rom_path, "rb"));
}
VMM_DEF bool vmm_insert_rom_w(const wchar_t *rom_path) {
    if (!rom_path) return false;
#ifdef _WIN32
    return _vmm_insert_rom(_wfopen(rom_path, L"rb"));
#else
    return false;
#endif
}
VMM_DEF void vmm_remove_rom(void) {
    if (_vmm.rom) {
        fclose(_vmm.rom);
        _vmm.rom = NULL;
        _vmm.rom_size = 0;
    }
}

VMM_DEF bool vmm_read_rom(dword addr, word chunk_id, size_t count) {
    if (!_vmm.rom) return false;

    for (size_t i = 0; (chunk_id+i) < VMM_MAX_CHUNK_COUNT && i < count; ++i) {
        vmm_chunk *chunk = &_vmm.chunks[chunk_id+i];
        if (chunk->addr != VMM_EMPTY_CHUNK) {
            if ((addr + _vmm.chunk_size*(i+1)) > _vmm.rom_size) {
                dword size = _vmm.rom_size - (addr + _vmm.chunk_size*i);

                fseek(_vmm.rom, addr + _vmm.chunk_size*i, SEEK_SET);
                fread(&_vmm.memory[chunk->addr], 1, size, _vmm.rom);

                break;
            } else {
                fseek(_vmm.rom, addr + _vmm.chunk_size*i, SEEK_SET);
                fread(&_vmm.memory[chunk->addr], 1, _vmm.chunk_size, _vmm.rom);
            }
        }
    }
}
VMM_DEF bool vmm_read_rom_from_array(dword addr, word chunk_id, size_t count, byte *rom, size_t rom_size) {
    if (!rom || !rom_size) return false;

    for (size_t i = 0; (chunk_id+i) < VMM_MAX_CHUNK_COUNT && i < count; ++i) {
        vmm_chunk *chunk = &_vmm.chunks[chunk_id+i];
        if (chunk->addr != VMM_EMPTY_CHUNK) {
            if ((addr + _vmm.chunk_size*(i+1)) > rom_size) {
                dword size = rom_size - (addr + _vmm.chunk_size*i);
                memcpy(&_vmm.memory[chunk->addr], &rom[addr + _vmm.chunk_size*i], size);

                break;
            } else memcpy(&_vmm.memory[chunk->addr], &rom[addr + _vmm.chunk_size*i], _vmm.chunk_size);
        }
    }
}

VMM_DEF inline void vmm_write(dword addr, byte value) {
    word chunk_id = addr >> _vmm.addr_shift;
    if (chunk_id >= VMM_MAX_CHUNK_COUNT) return;
    
    vmm_chunk *chunk = &_vmm.chunks[chunk_id];
    if (chunk->addr != VMM_EMPTY_CHUNK && !chunk->read_only)
        _vmm.memory[chunk->addr + (addr & _vmm.mask)] = value;
}
VMM_DEF inline byte vmm_read(dword addr) {
    word chunk_id = addr >> _vmm.addr_shift;
    if (chunk_id >= VMM_MAX_CHUNK_COUNT) return 0;
    
    dword chunk_addr = _vmm.chunks[chunk_id].addr;
    if (chunk_addr != VMM_EMPTY_CHUNK)
        return _vmm.memory[chunk_addr + (addr & _vmm.mask)];
    else
        return 0;
}

VMM_DEF vmm_chunk_ptr vmm_get_chunk(word chunk_id) {
    if (chunk_id >= VMM_MAX_CHUNK_COUNT) return NULL;

    dword chunk_addr = _vmm.chunks[chunk_id].addr;
    if (chunk_addr != VMM_EMPTY_CHUNK)
        return &_vmm.memory[chunk_addr];
    else
        return NULL;
}
#endif

#endif
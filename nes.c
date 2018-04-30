#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define KB32 (1 << 15)
#define KB16 (1 << 14)
#define KB8  (1 << 13)

#define HR_DATA "00"
#define HR_EOF  "01"
#define HR_DATA_SIZE (sizeof(char)*5 + sizeof(uint16_t) + 512) 
#define BYTE_COUNT 1

const char nibble_hex[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c',
    'd', 'e', 'f'
};

const char * _eof = ":00000001ff";

struct ines_header {
    char nes[4];
    uint8_t prg_rom;
    uint8_t chr_rom;
    uint8_t flags6;
    uint8_t flags7;
    uint8_t prg_ram;
    uint8_t flags9;
    uint8_t flags10;
    uint8_t zeros[5];
};

struct nes_mem {
    uint8_t empty[KB32];
    uint8_t prg_rom0[KB16];
    uint8_t prg_rom1[KB16];
};

struct dkong {
    uint8_t prg_rom0[KB16];
    uint8_t prg_rom1[KB16];
    uint8_t chr_rom[KB8];
};

struct hex_write {
    char start_code;
    char byte_count[2];
    char addr[4];
    char record_type[2];
    char data[BYTE_COUNT * 2];
    char checksum[2];
    char line_feed;
};


struct hex_record {
    uint8_t byte_count;
    uint16_t addr;
    uint8_t checksum;
    struct hex_write write;
};

void short_to_hex(void * dest, uint16_t val)
{
    /* High nibble first */
    uint8_t * dest8 = (uint8_t *)dest;
    dest8[0] = nibble_hex[ (val & 0xf000) >> 12 ];
    dest8[1] = nibble_hex[ (val & 0x0f00) >> 8 ];
    dest8[2] = nibble_hex[ (val & 0x00f0) >> 4 ];
    dest8[3] = nibble_hex[ (val & 0x000f) ];
}

void byte_to_hex(void * dest, uint8_t byte)
{
    /* High nibble first */
    uint8_t * dest8 = (uint8_t *)dest;
    dest8[0] = nibble_hex[ (byte & 0xf0) >> 4];
    dest8[1] = nibble_hex[ (byte & 0x0f) ];
}

void create_intel_hex(struct dkong * dkong)
{
    FILE * fd;
    if (NULL == (fd = fopen("dk_prg_rom.hex", "w+"))) {
        printf("Failed to open fd\n");
        return;
    }

    struct hex_record rec;
    rec.addr = 0;
    rec.write.start_code = ':';
    rec.write.line_feed = '\n';
    rec.write.record_type[1] = '0';
    rec.write.record_type[0] = '0';

    byte_to_hex(rec.write.byte_count, BYTE_COUNT);
    while (rec.addr < KB16) {
        rec.checksum = BYTE_COUNT +
            (uint8_t)rec.addr + 
            (uint8_t)( (rec.addr & 0xff00) >> 8);

        for (int i = 0; i < BYTE_COUNT; ++i) {
            byte_to_hex(rec.write.data + i*2,
                    dkong->prg_rom0[rec.addr + i]);
            rec.checksum += dkong->prg_rom0[rec.addr + i];
        }

        rec.checksum = (~rec.checksum) + 1;
        byte_to_hex(rec.write.checksum, rec.checksum);
        short_to_hex(rec.write.addr, rec.addr);

        if (1 != fwrite(&(rec.write), sizeof(struct hex_write), 1, fd)) {
            printf("Failed to write record at addr 0x%s", rec.write.addr);
        }

        rec.addr += BYTE_COUNT;
    }

    /* Write EOF record */
    if (1 != fwrite(_eof, 11, 1, fd)) {
        printf("Failed to write eof\n");
    }

close_exit:
    fclose(fd);
}

void create_files(struct dkong * dkong)
{
    FILE * fd = fopen("dk_prg0.ram", "w+");
    if (NULL == fd) {
        printf("Failed to open file.\n");
        return;
    }

    if (1 != fwrite( &(dkong->prg_rom0), KB16, 1, fd)) {
        printf("Failed to write prg_rom0");
        fclose(fd);
        return;
    }

    fclose(fd);

    fd = fopen("dk_prg1.ram", "w+");
    if (NULL == fd) {
        printf("Failed to open file.\n");
        return;
    }

    if (1 != fwrite( &(dkong->prg_rom1), KB16, 1, fd)) {
        printf("Failed to write prg_rom1");
        fclose(fd);
        return;
    }

    fclose(fd);
    fd = fopen("dk_chr.ram", "w+");
    if (NULL == fd) {
        printf("Failed to open file.\n");
        return;
    }

    if (1 != fwrite( &(dkong->chr_rom), KB16, 1, fd)) {
        printf("Failed to write chr_rom\n");
        fclose(fd);
        return;
    }

    fclose(fd);
    printf("Wrote all ram files\n");
}

void create_fat_file(struct dkong * dkong)
{
    struct nes_mem * nmem;
    if (NULL == (nmem = calloc(1, sizeof(struct nes_mem)))) {
        printf("Failed to allocate memory\n");
        return;
    }
    printf("Allocated nmem\n");
    memcpy(&(nmem->prg_rom0), &(dkong->prg_rom0), KB16);
    printf("Moved 16KB\n");
    memcpy(&(nmem->prg_rom1), &(dkong->prg_rom1), KB16);
    printf("Moved 16KB\n");

    FILE * fd;
    if (NULL == (fd = fopen("dk_64k", "w+"))) {
        printf("Failed to open file\n");
        goto free_exit;
    }
    printf("Opened fd\n");

    if (1 != fwrite( nmem, sizeof(struct nes_mem), 1, fd)) {
        printf("Failed to write dk_64\n");
    }

    printf("Wrote to fd\n");

    if (EOF == fclose(fd)) {
        printf("%s\n", strerror(errno));
    }
free_exit:
    free(nmem);
    printf("Freed nmem\n");
}


int main(void)
{
    int retval = 0;
    FILE * fd;
    if (NULL == (fd = fopen("/home/nathan/scripts/c/nes/dkong.nes", "r"))) {
        printf("Failed to open fd\n");
        retval = 1;
        goto clean_exit;
    }

    struct ines_header header;
    fread(&header, sizeof(struct ines_header), 1, fd);

    printf("Donkey Kong has:\nPRG_ROM: %d\nCHR_ROM: %d\nPRG_RAM: %d\n",
            header.prg_rom, header.chr_rom, header.prg_ram);

    struct dkong * dkong;
    if (NULL == (dkong = malloc(sizeof(struct dkong))))
        goto clean_exit;

   if (header.flags6 & 0x08) {
        printf("Trainer Present\n");
        goto clean_exit;
   } else {
       printf("No trainer\n");
   }

   int byte_read = fread(dkong, sizeof(struct dkong), 1, fd);

   printf("Read %d dkong\n", byte_read);
   create_intel_hex(dkong);
   //create_files(dkong);
   //create_fat_file(dkong);

clean_exit:
    if (NULL != dkong){
        free(dkong);
        printf("Freed dkong\n");
    }
    fclose(fd);
    return retval;
}

#include <stdint.h>
#include <display.h>
#include <stddef.h>
#include <string.h>

#include <acpi.h>
#include <multiboot2.h>
#include <system.h>
#include <serial.h>
#include <stdbool.h>

uint8_t acpi_revision = 1;

struct XSDP_t *acpi_xsdp = NULL;
struct ACPISDTHeader *acpi_rsdt = NULL;
struct FADT *acpi_fadt = NULL;

void *dsdt = NULL;

void acpi_error(char *message)
{
    serial_printf("ACPI ERROR: %s\n", message);
    kprintf("\033[0;31mACPI ERROR: %s\033[0m\n", message);
}

void acpi_warn(char *message)
{
    serial_printf("ACPI WARNING: %s\n", message);
    kprintf("\033[0;33mACPI WARNING: %s\033[0m\n", message);
}

uint32_t read_pkglength(uint8_t *where, uint8_t *len) {
    if (!(*where & 0b11000000)) {
        *len = 1;
        return *where;
    }

    uint8_t num_following = (*where & 0b11000000) >> 6;
    uint8_t shift = 4;

    uint32_t ret = 0;
    ret = *where & 0b00001111;
    for (uint8_t i = 0; i < num_following; i++) {
        ret |= where[i + 1] << shift;
        shift += 8;
    }

    *len = num_following + 1;

    return ret;
}

struct acpi_name read_name_length(uint8_t *where) {
    struct acpi_name ret;

    uint8_t read = 0;

    if (*where == '\\') {
        where++;
        read++;
    } else if (*where == '^') {
        where++;
        read++;
    }

    ret.offset = read;
    
    if (IS_LEADNAMECHAR(*where) && IS_NAMECHAR(where[1]) && IS_NAMECHAR(where[2]) && IS_NAMECHAR(where[3])) {
        read += 4;
        ret.type = 1;
        ret.namelen = 4;
    } else if (*where == 0x2E && IS_NAMECHAR(where[1]) && IS_NAMECHAR(where[2]) && IS_NAMECHAR(where[3]) && IS_LEADNAMECHAR(where[4]) && IS_NAMECHAR(where[5]) && IS_NAMECHAR(where[6]) && IS_NAMECHAR(where[7])) {
        // DualNamePrefix
        read += 9;
        ret.type = 2;
        ret.offset += 1;
        ret.namelen = 8;
    } else if (*where == 0x0) {
        // NullName
        read += 1;
        ret.type = 0;
        ret.offset += 1;
        ret.namelen = 0;
    } else if (*where == 0x2F) {
        // MultiNamePrefix
        read += 1;
        where++;
        uint8_t num = *where;
        read += 1 + (num * 4);
        ret.offset += 2;
        ret.type = 3;
        ret.namelen = num * 4;
    } else {
        acpi_error("Invalid name");
        ret.type = 0;
        ret.length = 0;
        return ret;
    }

    ret.length = read;

    return ret;
}

void print_name(uint8_t *where) {
    struct acpi_name name = read_name_length(where);
    if (name.type == 0) {
        serial_printf("(null)");
        return;
    }
    for (size_t i = 0; i < name.namelen; i++) {
        serial_printf("%c", where[i + name.offset]);
    }
}

uint32_t read_dataref_obj_length(uint8_t *where) {
    if (*where == 0x12) {
        where++;
        uint8_t len = 0;
        uint32_t ret = read_pkglength(where, &len);
        return ret + 1; // NumElements is 1 byte
    } else if (*where == 0) {
        return 1;
    } else {
        serial_printf("Invalid DataRefObj, cannot read length: 0x%x\n", *where);
        return 0;
    }
}

struct acpi_type read_dataref_obj_to_type(uint8_t *where) {
    if (*where == 0) {
        return (struct acpi_type){ACPI_TYPE_ZERO, where};
    } else {
        serial_printf("Invalid DataRefObj, cannot convert to type: 0x%x\n", *where);
        return (struct acpi_type){0, NULL};
    }
}

struct acpi_type read_nth_dataref_obj(uint8_t *where, uint8_t n) {
    if (*where == 0x12) {
        where++;
        uint8_t len = 0;
        read_pkglength(where, &len);
        where += len;
        uint8_t num_elems = *(where++);
        kprintf("NumElements: %d\n", num_elems);
        if (num_elems == 0 || n >= num_elems) {
            return (struct acpi_type){0, NULL};
        }
        for (size_t i = 0; i < n; i++) {
            uint8_t data_len = read_dataref_obj_length(where);
            where += data_len;
        }
        return read_dataref_obj_to_type(where); 
    } else {
        serial_printf("Invalid DataRefObj, cannot read to type: 0x%x\n", *where);
        return (struct acpi_type){0, NULL};
    }
}

uint64_t read_dataref_obj_to_uint64(uint8_t *where) {
    if (*where == 0) {
        return 0;
    } else {
        serial_printf("Invalid DataRefObj, cannot convert to uint64: 0x%x\n", *where);
        return 0;
    }
}






void *find_root_table(char *table, struct ACPISDTHeader *root) {
    if (acpi_revision == 1)
    {
        struct RSDT *rsdt = (struct RSDT *)(root);
        if (strncmp(rsdt->h.Signature, "RSDT", 4) != 0)
        {
            acpi_error("Invalid RSDT signature");
            return NULL;
        }

        uint32_t entries = (rsdt->h.Length - sizeof(struct ACPISDTHeader)) / 4;
        for (size_t i = 0; i < entries; i++)
        {
            struct ACPISDTHeader *header = (struct ACPISDTHeader *)(rsdt->PointerToOtherSDT[i] + VIRT_MEM_OFFSET);
            if (strncmp(header->Signature, table, 4) == 0)
            {
                return header;
            }
        }
    }
    else if (acpi_revision == 2)
    {
        struct XSDT *xsdt = (struct XSDT *)(root);
        if (strncmp(xsdt->h.Signature, "XSDT", 4) != 0)
        {
            acpi_error("Invalid XSDT signature");
            return NULL;
        }

        uint32_t entries = (xsdt->h.Length - sizeof(struct ACPISDTHeader)) / 8;
        for (size_t i = 0; i < entries; i++)
        {
            struct ACPISDTHeader *header = (struct ACPISDTHeader *)(xsdt->PointerToOtherSDT[i] + VIRT_MEM_OFFSET);
            if (strncmp(header->Signature, table, 4) == 0)
            {
                return header;
            }
        }
    }

    return NULL;
}

void acpi_init(uint32_t acpi_tag_addr)
{
    if (acpi_tag_addr == 0)
    {
        kprintf("ACPI not found\n");
        return;
    }

    uint32_t *acpi_tag = (uint32_t *)(acpi_tag_addr + VIRT_MEM_OFFSET);

    if (*acpi_tag == MULTIBOOT_TAG_TYPE_ACPI_NEW)
    {
        acpi_revision = 2;
    }

    // entire copy located immediately after the tag's 2 uint32_t header
    acpi_xsdp = (struct XSDP_t *)(acpi_tag + 2);

    // print out the fields
    kprintf("Signature: %c%c%c%c%c%c%c%c\n", acpi_xsdp->Signature[0], acpi_xsdp->Signature[1], acpi_xsdp->Signature[2], acpi_xsdp->Signature[3], acpi_xsdp->Signature[4], acpi_xsdp->Signature[5], acpi_xsdp->Signature[6], acpi_xsdp->Signature[7]);
    kprintf("Checksum: %d\n", acpi_xsdp->Checksum);
    kprintf("OEMID: %c%c%c%c%c%c\n", acpi_xsdp->OEMID[0], acpi_xsdp->OEMID[1], acpi_xsdp->OEMID[2], acpi_xsdp->OEMID[3], acpi_xsdp->OEMID[4], acpi_xsdp->OEMID[5]);
    kprintf("Revision: %d\n", acpi_xsdp->Revision);
    kprintf("RsdtAddress: %x\n", acpi_xsdp->RsdtAddress);
    if (acpi_revision == 2)
    {
        kprintf("Length: %d\n", acpi_xsdp->Length);
        kprintf("XsdtAddress: %lx\n", acpi_xsdp->XsdtAddress);
        kprintf("ExtendedChecksum: %d\n", acpi_xsdp->ExtendedChecksum);
    }

    if (strncmp(acpi_xsdp->Signature, "RSD PTR ", 8) != 0)
    {
        acpi_error("Invalid RSDP signature");
        return;
    }
    
    // verify checksum
    uint8_t sum = 0;
    for (size_t i = 0; i < 20; i++)
    {
        sum += ((uint8_t *)acpi_xsdp)[i];
    }
    if (sum != 0)
    {
        acpi_error("Invalid checksum");
        return;
    }

    acpi_rsdt = (struct ACPISDTHeader *)(acpi_xsdp->RsdtAddress + VIRT_MEM_OFFSET);

    if (strncmp(acpi_rsdt->Signature, "RSDT", 4) != 0)
    {
        acpi_error("Invalid RSDT signature");
        return;
    }

    kprintf("ACPI revision: %d\n", acpi_revision);
    kprintf("ACPI RSDT address: %x\n", acpi_rsdt);

    acpi_fadt = (struct FADT *)find_root_table("FACP", acpi_rsdt);

    if (acpi_fadt == NULL)
    {
        acpi_error("FADT not found");
        return;
    }

    if (acpi_revision == 1)
    {
        dsdt = (void *)(acpi_fadt->Dsdt + VIRT_MEM_OFFSET);
    }
    else if (acpi_revision == 2)
    {
        // need to use x_dsdt
        dsdt = (void *)(acpi_fadt->X_Dsdt + VIRT_MEM_OFFSET);
    }
}

void *read_aml_path(char *path, void *aml_ptr, uint32_t length) {
    // get the first part, before the dot, of the path
    // no stdlib, so we have to do this manually
    uint8_t *ptr = (uint8_t *)aml_ptr;
    uint8_t *end = ptr + length;

    // don't modify the original path
    serial_printf("Path: %s\n", path);
    if (path[0] == '\\') {
        path++;
    }

    // locate dot
    char *dot = path;
    while (*dot != 0 && *dot != '.') {
        dot++;
    }
    if (*dot == 0) {
        // no dot found, so we're at the end of the path
        dot = NULL;
    } else {
        // replace the dot with a null terminator
        *dot = 0;
    }

    serial_printf("First part: %s\n", path);
    serial_printf("Second part: %s\n", dot == NULL ? "" : dot + 1);

    // now search for the first part, scopes only
    while (ptr < end) {
        if (*ptr == 0x10) {
            // ScopeOp
            ptr++;
            uint8_t name_len = 0;
            uint32_t pkg_len = read_pkglength(ptr, &name_len);
            uint8_t *object_end = ptr + pkg_len;
            ptr += name_len;
            struct acpi_name name = read_name_length(ptr);
            
            serial_printf("ScopeOp: ");
            print_name(ptr);
            serial_printf("\n");

            serial_printf("Comparing %s to ", path);
            for (size_t i = 0; i < name.namelen; i++) {
                serial_printf("%c", ((char *)ptr)[name.offset + i]);
            }
            serial_printf(": %s\n", strncmp(path, (char *)ptr + name.offset, strlen(path)) == 0 && name.namelen == strlen(path) ? "true" : "false");



            if (strncmp(path, (char *)ptr + name.offset, strlen(path)) == 0 && name.namelen == strlen(path)) {
                // if this is the end of the path, return the object
                if (dot == NULL) {
                    return ptr + name.length;
                } else {
                    // otherwise, recurse into the scope
                    kprintf("ScopeOp: Recursing with new path %s\n", dot + 1);
                    void *ret = read_aml_path(dot + 1, ptr + name.length, pkg_len - name.length);
                    if (ret != NULL) {
                        return ret;
                    }
                    kprintf("Not found in there. Going back to path %s\n", path);
                }
            }

            ptr += name.length;
            ptr = object_end;
        } else if (*ptr == 0x14) {
            // MethodOp
            ptr++;
            uint8_t name_len = 0;
            uint32_t pkg_len = read_pkglength(ptr, &name_len);
            uint8_t *object_end = ptr + pkg_len;
            ptr += name_len;
            struct acpi_name name = read_name_length(ptr);

            serial_printf("MethodOp: ");
            print_name(ptr);
            serial_printf("\n");

            if (strncmp(path, (char *)ptr + name.offset, strlen(path)) == 0 && name.namelen == strlen(path)) {
                // if this is the end of the path, return the object
                if (dot == NULL) {
                    return ptr + name.length;
                }
            }

            ptr += name.length;
            ptr = object_end;
        } else if (*ptr == 0x08) {
            // NameOp
            ptr++;
            struct acpi_name name = read_name_length(ptr);
            serial_printf("NameOp [%d]: ", name.length);
            print_name(ptr);
            serial_printf("\n");

            if (strncmp(path, (char *)ptr + name.offset, strlen(path)) == 0 && name.namelen == strlen(path)) {
                // if this is the end of the path, return the object
                if (dot == NULL) {
                    return ptr + name.length;
                }
            }

            ptr += name.length;

            uint32_t dataref_len = read_dataref_obj_length(ptr);
            ptr += dataref_len;
        } else {
            serial_printf("\033[0;31mInvalid AML opcode: 0x%x 0x%x\033[0m\n", *ptr, *(ptr + 1));
            return NULL;
        }
    }

    return NULL;
}





void memdump(void *ptr, size_t len) {
    // dump in the format:
    // 0x%lx: aa, bb, cc, dd, ee, ff, gg, hh, ii, jj, kk, ll, mm, nn, oo, pp    |abcdefghijklmnop|
    uint8_t *p = (uint8_t *)ptr;
    size_t i = 0;

    // note that entries should be padded to 2 characters (eg 0F), and the last line should be padded with spaces
    while (i < len) {
        serial_printf("0x%lx: ", (uint64_t)p);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                serial_printf("%s%x ", p[j] < 0x10 ? "0" : "", p[j]);
            } else {
                serial_printf("   ");
            }
        }
        serial_printf(" |");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                if (p[j] >= 0x20 && p[j] <= 0x7E) {
                    serial_printf("%c", p[j]);
                } else {
                    serial_printf(".");
                }
            } else {
                serial_printf(" ");
            }
        }
        serial_printf("|\n");

        i += 16;
        p += 16;
    }
}



void acpi_test() {
    void *sbloc = read_aml_path("\\._S5_", dsdt + sizeof(struct ACPISDTHeader), ((struct ACPISDTHeader *)dsdt)->Length - sizeof(struct ACPISDTHeader));
    if (sbloc == NULL) {
        acpi_error("Failed to find _S5_ :(");
        return;
    }
    kprintf("\033[0;32mFound _S5_ :D @ 0x%lx\033[0m\n", (uint64_t)sbloc);
    kprintf("First 12 bytes {\n\t");
    for (size_t i = 0; i < 12; i++) {
        kprintf("0x%x ", ((uint8_t *)sbloc)[i]);
    }
    kprintf("\n}\n");

    kprintf("PM1a_CNT_BLK: 0x%x\n", acpi_fadt->PM1aControlBlock);
    kprintf("PM1b_CNT_BLK: 0x%x\n", acpi_fadt->PM1bControlBlock);
    kprintf("X_PM1a_CNT_BLK: 0x%lx\n", acpi_fadt->X_PM1aControlBlock.Address);
    kprintf("X_PM1b_CNT_BLK: 0x%lx\n", acpi_fadt->X_PM1bControlBlock.Address);

    // struct acpi_type obj = read_nth_dataref_obj(sbloc, 0);

    // read objects 0 and 1 to uint64
    uint64_t obj0 = read_dataref_obj_to_uint64(read_nth_dataref_obj(sbloc, 0).data);
    uint64_t obj1 = read_dataref_obj_to_uint64(read_nth_dataref_obj(sbloc, 1).data);
    kprintf("Obj0: 0x%lx\n", obj0);
    kprintf("Obj1: 0x%lx\n", obj1);

    // SHUT IT DOWN :D
    outw(acpi_fadt->PM1aControlBlock, obj0 | (1 << 13));
}
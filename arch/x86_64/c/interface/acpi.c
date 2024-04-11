#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <display.h>
#include <multiboot2.h>
#include <errors.h>
#include <acpi.h>
#include <string.h>
#include <system.h>

bool acpi_20 = false;
bool acpi_available = false;

char *errmsg = "ACPI uninitialized\n";

void *XSDT = NULL; // RSDT or XSDT, depending on version
void *FADT = NULL;
void *DSDT = NULL;

void bad_acpi(char *msg) {
    // free memory (when implemented)

    errmsg = msg;
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

void print_tabs(uint32_t indent) {
    for (uint32_t i = 0; i < indent; i++) {
        serial_printf("\t");
    }
}

#define IS_LEADNAMECHAR(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
#define IS_DIGIT(c) (c >= '0' && c <= '9')
#define IS_NAMECHAR(c) (IS_LEADNAMECHAR(c) || IS_DIGIT(c))

void print_name(char *name, uint8_t *read, bool is_supername) {
    *read = 0;

    if (*name == '\\') {
        name++;
        *read = 1;
    } else if (*name == '^') {
        name++;
        *read = 1;
    }

    if (IS_LEADNAMECHAR(*name) && IS_NAMECHAR(name[1]) && IS_NAMECHAR(name[2]) && IS_NAMECHAR(name[3])) {
        *read += 4;
        serial_printf("%c%c%c%c", name[0], name[1], name[2], name[3]);
    } else if (*name == 0x2E && IS_NAMECHAR(name[1]) && IS_NAMECHAR(name[2]) && IS_NAMECHAR(name[3]) && IS_LEADNAMECHAR(name[4]) && IS_NAMECHAR(name[5]) && IS_NAMECHAR(name[6]) && IS_NAMECHAR(name[7])) {
        // DualNamePrefix
        *read += 9;
        serial_printf("%c%c%c%c%c%c%c%c", name[0], name[1], name[2], name[3], name[4], name[5], name[6], name[7]);
    } else if (*name == 0x0) {
        // NullName
        *read += 1;
        serial_printf("NULL");
    } else {
        if (is_supername) {
            // may be a few other things
            if (*name >= 0x68 && *name <= 0x6E) {
                *read += 1;
                serial_printf("Arg%d", *name - 0x68);
                return;
            } else if (*name >= 0x60 && *name <= 0x67) {
                *read += 1;
                serial_printf("Local%d", *name - 0x60);
                return;
            }
        }

        kpanic("Invalid AML name: %d\n", *name);
    }
}

void parse_termarg(uint8_t *ptr, uint8_t *read) {
    serial_printf("(");
    uint8_t read_skip;
    if (*ptr == 0xA) {
        ptr++;
        *read = 2;
        serial_printf("ByteData:0x%x", *ptr);
    } else if (*ptr == 0xB) { // WordPrefix
        ptr++;
        *read = 3;
        serial_printf("WordData:0x%x", *((uint16_t *)ptr));
    } else if (*ptr == 0x0) { // ZeroOp
        *read = 1;
        serial_printf("ZeroOp:0");
    } else if (*ptr == 0x1) { // OneOp
        *read = 1;
        serial_printf("OneOp:1");
    } else if (*ptr >= 0x68 && *ptr <= 0x6E) {
        *read = 1;
        serial_printf("Arg:%d", *ptr - 0x68);
    } else if (*ptr >= 0x60 && *ptr <= 0x67) {
        *read = 1;
        serial_printf("Local:%d", *ptr - 0x60);
    } else if (*ptr == 0x83) { // DerefOfOp
        ptr++;
        *read = 1;
        serial_printf("DerefOf(");
        parse_termarg(ptr, &read_skip);
        *read += read_skip;
        serial_printf(")");
    } else if (*ptr == 0x87) { // SizeOfOp
        ptr++;
        *read = 1;
        serial_printf("sizeof(");
        print_name((char *)ptr, &read_skip, true);
        *read += read_skip;
        serial_printf(")");
    } else if (*ptr == 0x88) { //IndexOp
        ptr++;
        *read = 1;
        serial_printf("Index(");
        parse_termarg(ptr, &read_skip);
        *read += read_skip;
        ptr += read_skip;
        serial_printf(", ");
        parse_termarg(ptr, &read_skip);
        *read += read_skip;
        serial_printf(" -> ");
        print_name((char *)ptr, &read_skip, true);
        *read += read_skip;
    } else if (*ptr == 0x95) { // LlessOp
        ptr++;
        *read = 1;
        parse_termarg(ptr, &read_skip);
        ptr += read_skip;
        *read += read_skip;
        serial_printf(" < ");
        parse_termarg(ptr, &read_skip); 
        *read += read_skip;
    } else {
        serial_printf("? invalid termarg 0x%x\n", *ptr);
        kpanic("invalid termarg\n");
    }

    serial_printf(") ");
}

void dump_aml_at(uint8_t *ptr, uint32_t indent, uint64_t num_to_read) {
    uint64_t end = (uint64_t)ptr + num_to_read;

    uint8_t read_skip;
    uint32_t pkglen;

    uint64_t item_end;

    while ((uint64_t)ptr <= end) {
        print_tabs(indent);
        if (ptr[0] == 0x10) { // ScopeOp
            ptr++;
            pkglen = read_pkglength(ptr, &read_skip);
            item_end = (uint64_t)ptr + pkglen;
            ptr += read_skip;
            serial_printf("ScopeOp [%d] ", pkglen);
            print_name((char *)ptr, &read_skip, false);
            ptr += read_skip;
            serial_printf("\n");
            dump_aml_at(ptr, indent + 1, item_end - (uint64_t)ptr - 1);
            ptr = (uint8_t *)item_end;
        } else if (ptr[0] == 0x14) { // MethodOp
            ptr++;
            pkglen = read_pkglength(ptr, &read_skip);
            item_end = (uint64_t)ptr + pkglen;
            ptr += read_skip;
            serial_printf("MethodOp [%d] ", pkglen);
            print_name((char *)ptr, &read_skip, false);
            ptr += read_skip;
            serial_printf(" Flags: %d ", ptr[0]);
            ptr++;
            serial_printf("\n");
            dump_aml_at(ptr, indent + 1, item_end - (uint64_t)ptr - 1);
            ptr = (uint8_t *)item_end;
        } else if (ptr[0] == 0x5B) { // ExtendedOpPrefix
            if (ptr[1] == 0x80) { // RegionOp
                ptr += 2;
                serial_printf("RegionOp ");
                print_name((char *)ptr, &read_skip, false);
                ptr += read_skip;
                serial_printf(" Space: %d ", ptr[0]);
                ptr++;
                serial_printf("Offset: ");
                parse_termarg(ptr, &read_skip);
                ptr += read_skip;
                serial_printf(" Length: ");
                parse_termarg(ptr, &read_skip);
                ptr += read_skip;
                serial_printf("\n");
            } else if (ptr[1] == 0x81) { // FieldOp
                ptr += 2;
                pkglen = read_pkglength(ptr, &read_skip);
                item_end = (uint64_t)ptr + pkglen;
                ptr += read_skip;
                serial_printf("FieldOp [%d] ", pkglen);
                print_name((char *)ptr, &read_skip, false);
                ptr += read_skip;
                serial_printf(" Flags: %d ", ptr[0]);
                ptr++;
                // just skip the rest for now
                ptr = (uint8_t *)item_end;
                serial_printf("skipping remainder\n");
            } else {
                serial_printf("Unknown AML extended opcode 0x%x\n", ptr[1]);
                break;
            }
        } else if (ptr[0] == 0x70) { // StoreOp
            ptr++;
            serial_printf("StoreOp: ");
            parse_termarg(ptr, &read_skip);
            ptr += read_skip;
            print_name((char *)ptr, &read_skip, true);
            ptr += read_skip;
            serial_printf("\n");
        } else if (ptr[0] == 0x74) { // SubtractOp
            ptr++;
            serial_printf("SubtractOp: ");
            parse_termarg(ptr, &read_skip);
            ptr += read_skip;
            parse_termarg(ptr, &read_skip);
            ptr += read_skip;
            print_name((char *)ptr, &read_skip, true);
            ptr += read_skip;
            serial_printf("\n");
        } else if (ptr[0] == 0x75) { // IncrementOp
            ptr++;
            serial_printf("IncrementOp: ");
            print_name((char *)ptr, &read_skip, true);
            ptr += read_skip;
            serial_printf("\n");
        } else if (ptr[0] == 0x96) { // ToBufferOp
            ptr++;
            serial_printf("ToBufferOp: ");
            parse_termarg(ptr, &read_skip);
            ptr += read_skip;
            print_name((char *)ptr, &read_skip, true);
            ptr += read_skip;
            serial_printf("\n");
        } else if (ptr[0] == 0x98) { // ToHexStringOp
            ptr++;
            serial_printf("ToHexStringOp: ");
            parse_termarg(ptr, &read_skip);
            ptr += read_skip;
            print_name((char *)ptr, &read_skip, true);
            ptr += read_skip;
            serial_printf("\n");
        } else if (ptr[0] == 0xA2) { // WhileOp
            ptr++;
            pkglen = read_pkglength(ptr, &read_skip);
            item_end = (uint64_t)ptr + pkglen;
            ptr += read_skip;
            serial_printf("WhileOp [%d]: Predicate ", pkglen);
            parse_termarg(ptr, &read_skip);
            ptr += read_skip;
            serial_printf("\n");
            dump_aml_at(ptr, indent + 1, item_end - (uint64_t)ptr - 1);
            ptr = (uint8_t *)item_end;
        } else {
            serial_printf("Unknown AML opcode 0x%x\n", *ptr);
            break;
        }
    }
}

void acpi_init(uint64_t acpi_tag) {
    struct multiboot_tag_new_acpi *tag = (struct multiboot_tag_new_acpi *)acpi_tag;

    acpi_rsdp_t *acpi_rsdp = (void *)tag->rsdp;

    if (acpi_rsdp->revision == 0) {
        // validate checksum
        uint8_t sum = 0;
        for (size_t i = 0; i < sizeof(char) * 8 + sizeof(uint8_t) + sizeof(char) * 6 + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t); i++) {
            sum += ((uint8_t *)acpi_rsdp)[i];
        }
        if (sum != 0) {
            bad_acpi("ACPI checksum failed\n");
            return;
        }
    } else if (acpi_rsdp->revision != 2) {
        bad_acpi("ACPI version not supported\n");
        return;
    } else {
        acpi_20 = true;

        // validate checksum
        uint8_t sum = 0;
        for (size_t i = 0; i < sizeof(acpi_rsdp_t); i++) {
            sum += ((uint8_t *)acpi_rsdp)[i];
        }
        if (sum != 0) {
            bad_acpi("ACPI checksum failed\n");
            return;
        }
    }

    // make sure the signature makes sense
    if (strncmp(acpi_rsdp->signature, "RSD PTR ", 8) != 0) {
        bad_acpi("ACPI signature invalid\n");
        return;
    }

    if (acpi_20) {
        XSDT = (void *)(acpi_rsdp->xsdt_address + VIRT_MEM_OFFSET);
    } else {
        XSDT = (void *)(acpi_rsdp->rsdt_address + VIRT_MEM_OFFSET);
    }

    printf("RSDT/XSDT at 0x%lx\n", XSDT);

    // locate the FADT
    if (acpi_20) {
        kpanic(".");
    } else {
        acpi_rsdt_t *rsdt = XSDT;
        for (size_t i = 0; i < (rsdt->h.Length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t); i++) {
            acpi_sdt_header_t *header = (void *)(rsdt->other[i] + VIRT_MEM_OFFSET);
            if (strncmp(header->Signature, "FACP", 4) == 0) {
                FADT = (acpi_fadt_t *)header;
                break;
            }
        }
        
        if (FADT == NULL) {
            bad_acpi("FADT not found\n");
            return;
        }

        DSDT = (void *)(((acpi_fadt_t *)FADT)->Dsdt + VIRT_MEM_OFFSET);

        if (DSDT == NULL) {
            bad_acpi("DSDT not found\n");
            return;
        }
    }

    printf("FADT at 0x%lx\n", FADT);
    printf("DSDT at 0x%lx\n", DSDT);

    // TEMP: dump the whole DSDT to serial
    dump_aml_at(DSDT + sizeof(acpi_sdt_header_t), 0, ((acpi_sdt_header_t *)DSDT)->Length - sizeof(acpi_sdt_header_t));
}
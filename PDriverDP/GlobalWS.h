#ifndef PDRIVERDP_GLOBALWS_H
#define PDRIVERDP_GLOBALWS_H

#include <stdint.h>

#include "Core/Workspace.h"
#include "PDriverDP/Constants.h"
#include "PDriverDP/PDumper.h"

/* PDriverDP.GlobalWS -> C */

#define dp_data_dlm 12u



class PDriverWS : public CoreWS
{
public:
    PDriverWS(void* pw);
    ~PDriverWS();

private:
    PDriverWS(const PDriverWS&);
    PDriverWS& operator=(const PDriverWS&);



public:



    static PDriverWS& instance() { return *(PDriverWS*)Module::instance(); }

    uint8_t printer_dump_depth;
    uint8_t printer_interlace;
    uint8_t printer_x_interlace;
    uint8_t printer_passes_per_line;
    uint8_t printer_strip_type;
    uint8_t printer_output_bpp;
    uint8_t printer_no_passes;
    uint8_t printer_pass;

    uint32_t printer_pdumper_word;

    /* PDumper string information. */
    uint8_t printer_strings[244];
/* FIXME: 256 bytes before this are passed in via configure_setdriver(). need separate struct */

    uint32_t printer_configureword;
    PDumper* printer_pdumper_pointer;

    // PDumperXX number we are currently using, otherwise -1. This can be
    // set to a valid number even if printer_pdumper_pointer is nullptr.
    int32_t printer_pdumper_number;

    uint32_t dummy_leftmargin;

    uint32_t printer_stringblocksize;
    void* printer_stringblockptr;

    char pending_pdumper_command[256];
    uint32_t pending_info_flag;

    uint8_t pending_info_data[256]; // FIXME: Similarly, this is set by configure_setdriver().

    Utils::List<PDumper> pdumper_list;
};

#endif

#include "GlobalWS.h"

PDriverWS::PDriverWS(void* pw)
    : CoreWS(pw)
    , printer_pdumper_pointer(0)
    , printer_pdumper_number(-1) // undefined.
    , printer_stringblocksize(0)
    , printer_stringblockptr(nullptr)
{
    // Move these to the constructor once they've been reviewed.
    printer_dump_depth = 0;
    printer_interlace = 0;
    printer_x_interlace = 0;
    printer_passes_per_line = 0;
    printer_strip_type = 0;
    printer_output_bpp = 0;
    printer_no_passes = 0;
    printer_pass = 0;
    printer_pdumper_word = 0;

    memset(printer_strings, 0, sizeof(printer_strings));

    printer_configureword = 0;
    dummy_leftmargin = 0;
    printer_stringblocksize = 0;
    printer_stringblockptr = nullptr;
    memset(pending_pdumper_command, 0, sizeof(pending_pdumper_command));
    pending_info_flag = 0;
    memset(pending_info_data, 0, sizeof(pending_info_data));
}

PDriverWS::~PDriverWS()
{
    if (printer_stringblocksize != 0) {
        void *block = printer_stringblockptr;
        (void)rma_free(block);
    }

    PDumper* dumper;
    while ((dumper = pdumper_list.detachHead()) != nullptr)
        delete dumper;
}

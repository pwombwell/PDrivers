#pragma once

#include "RLib.h"

#include <stddef.h>

namespace riscos {
namespace ColourTrans {

// Allows pointers to an opaque ColourTrans block. Only the caller can know
// the format of the table and its subclass.
struct Table {
protected:
    Table() = default;
};

// Lookup table for original-style ColourTrans blocks. This block is the table.
struct ByteTable : Table
{
    ByteTable() { }

    // ColourTrans writes the table bytes directly into this block.
    // The meaning of those bytes depends on the table format requested.
    const uint8_t* bytes() const { return reinterpret_cast<const uint8_t*>(this); }
    uint8_t* bytes() { return reinterpret_cast<uint8_t*>(this); }

    // Valid only for greyscale translation tables: source pixel value -> grey.
    const uint8_t* greyBytes() const { return bytes(); }
    uint8_t* greyBytes() { return bytes(); }
};

// ColourTrans 32K table descriptor.
//
// Since ColourTrans 1.86, a 32K-style table descriptor may be either:
//   { "32K.", table pointer, "32K." }
// or:
//   { "32K+", table pointer, "32K+" }
//
// The descriptor is not the lookup table.  The middle word points at the
// start of the lookup table.  For a "32K+" table, a header immediately
// precedes the lookup table and describes the source pixel format.
//
// The contained table is not released by this class. If required, subclass.
struct Table32K : Table
{
    Table32K() : m_guard1(0), m_table(nullptr), m_guard2(0) { }

    const ByteTable* table() const { return m_table; }
    ByteTable* table() { return m_table; }

    const uint8_t* bytes() const { return m_table ? m_table->bytes() : nullptr; }
    uint8_t* bytes() { return m_table ? m_table->bytes() : nullptr; }

    const uint8_t* greyBytes() const { return bytes(); }
    uint8_t* greyBytes() { return bytes(); }

protected:
    void setTable(ByteTable* table) { m_table = table; }
    void setGuardWords(uint32_t guard) { m_guard1 = guard; m_guard2 = guard; }

protected:
    // ColourTrans layout.
    uint32_t    m_guard1;
    ByteTable*  m_table;
    uint32_t    m_guard2;
};

} } // namespace riscos::ColourTrans

#ifndef RLIBX_UTILS_ENUMBITMASKOPS_H
#define RLIBX_UTILS_ENUMBITMASKOPS_H

#define DEFINE_ENUM_BITWISE_OPERATORS(EnumType)                           \
    inline EnumType operator|(EnumType a, EnumType b)                     \
    { return EnumType(unsigned(a) | unsigned(b)); }                       \
                                                                          \
    inline EnumType operator&(EnumType a, EnumType b)                     \
    { return EnumType(unsigned(a) & unsigned(b)); }                       \
                                                                          \
    inline EnumType operator^(EnumType a, EnumType b)                     \
    { return EnumType(unsigned(a) ^ unsigned(b)); }                       \
                                                                          \
    inline EnumType operator~(EnumType a)                                 \
    { return EnumType(~unsigned(a)); }                                    \
                                                                          \
    inline EnumType& operator|=(EnumType& a, EnumType b)                  \
    { a = EnumType(a | b); return a; }                                              \
                                                                          \
    inline EnumType& operator&=(EnumType& a, EnumType b)                  \
    { a = EnumType(a & b); return a; }                                              \
                                                                          \
    inline EnumType& operator^=(EnumType& a, EnumType b)                  \
    { a = EnumType(a ^ b); return a; }                                              \
    static_assert(true, "require semi-colon")

#endif

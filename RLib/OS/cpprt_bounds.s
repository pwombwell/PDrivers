        AREA |C$$code|, CODE, READONLY

        IMPORT |C$$ctorvec$$Base|, WEAK
        IMPORT |C$$ctorvec$$Limit|, WEAK
        IMPORT |C$$dtorvec$$Base|, WEAK
        IMPORT |C$$dtorvec$$Limit|, WEAK

        EXPORT __cpp_ctorvec_base
        EXPORT __cpp_ctorvec_limit
        EXPORT __cpp_dtorvec_base
        EXPORT __cpp_dtorvec_limit

__cpp_ctorvec_base
        DCD     |C$$ctorvec$$Base|

__cpp_ctorvec_limit
        DCD     |C$$ctorvec$$Limit|

__cpp_dtorvec_base
        DCD     |C$$dtorvec$$Base|

__cpp_dtorvec_limit
        DCD     |C$$dtorvec$$Limit|

        END

/*!
 * @brief For Windows compatibility
 */
#ifdef _WIN32
#define strtok_r strtok_s
#endif

/*!
 * @brief POSIX suggests a line length of 4096,
 */
#define MAXLEN_LINE 4096

/*!
 * @brief Define comment for ASM files
 */
#define COMMENT ";"

/*!
 * @brief BSS begining of block
 */
#define BSS_START \
    ".RAMSECTION \".bss\" BANK $7e SLOT 2"
/*!
 * @brief BSS end of block
 */
#define BSS_END ".ENDS"

/*!
 * @brief Stores (accu/x/y/zero) to pseudo-registers
 */
#define STORE_AXYZ_TO_PSEUDO \
    "st([axyz]).b tcc__([rf][0-9]{0,}h{0,1})$"
/*!
 * @brief Stores (x/y) to pseudo-registers
 */
#define STORE_XY_TO_PSEUDO \
    "st([xy]).b tcc__([rf][0-9]{0,}h{0,1})$"
/*!
 * @brief Stores (accu only) to pseudo-registers
 */
#define STORE_A_TO_PSEUDO \
    "sta.b tcc__([rf][0-9]{0,}h{0,1})$"

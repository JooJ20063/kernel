#ifndef CZK_UAPI_FS_H
#define CZK_UAPI_FS_H

/*
 * Stable file-property bits returned in czk_stat_t.st_flags and
 * czk_dirent_t.d_flags. These are UAPI values, not kernel fs_node flags.
 */
#define CZK_FS_FILE      0x01U
#define CZK_FS_DIRECTORY 0x02U
#define CZK_FS_WRITABLE  0x04U
#define CZK_FS_SEEKABLE  0x08U

#endif

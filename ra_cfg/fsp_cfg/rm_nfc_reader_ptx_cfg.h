/* generated configuration header file - do not edit */
#ifndef RM_NFC_READER_PTX_CFG_H_
#define RM_NFC_READER_PTX_CFG_H_
#ifdef __cplusplus
extern "C" {
#endif

#define NFC_READER_PTX_CFG_PARAM_CHECKING_ENABLED           ((BSP_CFG_PARAM_CHECKING_ENABLE))

#define RM_NFC_READER_NDEF_SUPPORT                          (0)

#define RM_NFC_READER_NATIVE_TAG_SUPPORT                    (0)

#if (RM_NFC_READER_NDEF_SUPPORT)
#define NFC_READER_PTX_NDEF_WORK_BUF_SIZE                   (64)
#else
#define NFC_READER_PTX_NDEF_WORK_BUF_SIZE                   (1)
#endif

#ifdef __cplusplus
}
#endif
#endif /* RM_NFC_READER_PTX_CFG_H_ */

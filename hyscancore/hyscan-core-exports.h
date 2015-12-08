#ifndef __HYSCAN_CORE_EXPORTS_H__
#define __HYSCAN_CORE_EXPORTS_H__

#if defined (_WIN32)
  #if defined (hyscancore_EXPORTS)
    #define HYSCAN_CORE_EXPORT __declspec (dllexport)
  #else
    #define HYSCAN_CORE_EXPORT __declspec (dllimport)
  #endif
#else
  #define HYSCAN_CORE_EXPORT
#endif

#endif /* __HYSCAN_CORE_EXPORTS_H__ */

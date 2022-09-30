#include <cutils/properties.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LGDATA_VALUE_MAX PROPERTY_VALUE_MAX
#define LGDATA_SLOT_PRIMARY 0
#define LGDATA_SLOT_SECONDARY 1

bool LGDataFeature_is_operator(const char* _operator, int slotId);
bool LGDataFeature_is_country(const char* country,  int slotId);

bool LGDataFeature_is_const_operator(const char* _operator);
bool LGDataFeature_is_const_country(const char* country);
bool LGDataFeature_is_const_region(const char* region);

int LGDataFeature_get_operator(char* _operator, int slotId);
int LGDataFeature_get_country(char* country, int slotId);

int LGDataFeature_get_const_operator(char* _operator);
int LGDataFeature_get_const_country(char* country);
int LGDataFeature_get_const_region(char* region);

bool LGDataFeature_is_kr_sim_operator(const char* _operator);
bool LGDataFeature_is_jp_sim_operator(const char* _operator);

int LGDataFeature_get_kr_sim_operator(char* _operator);
int LGDataFeature_get_jp_sim_operator(char* _operator);

bool LGDataFeature_is_const_global_operator();

#ifdef __cplusplus
}//extern "C"
#endif

